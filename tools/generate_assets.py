#!/usr/bin/env python3
"""Generate private device assets from files you supply locally."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import tempfile
import zipfile
from PIL import Image, UnidentifiedImageError

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / '.local' / 'assets'
TRANSPARENT = 0xF81F
SOUND_NAMES = ('wing', 'point', 'hit', 'die', 'swoosh')
MAX_IMAGE_BYTES = 16 * 1024 * 1024
MAX_MANIFEST_BYTES = 1024 * 1024
MAX_AUDIO_BYTES = 8 * 1024 * 1024
MAX_IMAGE_PIXELS = 16 * 1024 * 1024
MAX_APK_BYTES = 256 * 1024 * 1024
MAX_APK_ENTRIES = 8192
MAX_SOUND_SECONDS = 30

# The renderer expects these source dimensions and atlas entry names.
REQUIRED_SIZES = {
    'bg_day': (288, 512), 'land': (336, 112),
    'pipe_up': (52, 320), 'pipe_down': (52, 320),
    'bird0_0': (48, 48), 'bird0_1': (48, 48), 'bird0_2': (48, 48),
    'title': (178, 48), 'text_ready': (196, 62), 'tutorial': (114, 98),
    'text_game_over': (204, 54), 'score_panel': (238, 126),
    'button_play': (116, 70), 'new': (32, 14),
}
REQUIRED_SIZES.update({f'font_{48 + i:03d}': (16 if i == 1 else 24, 44) for i in range(10)})
REQUIRED_SIZES.update({f'number_context_{i:02d}': (12, 14) for i in range(10)})
REQUIRED_SIZES.update({f'number_score_{i:02d}': (16, 20) for i in range(10)})
REQUIRED_SIZES.update({f'medals_{i}': (44, 44) for i in range(4)})


class AssetError(ValueError):
    """A supplied asset cannot be used by this renderer."""


def read_local(path: Path, limit: int) -> bytes:
    if not path.is_file():
        raise AssetError(f'Local input is not a file: {path}')
    if path.stat().st_size > limit:
        raise AssetError(f'Local input exceeds the {limit}-byte size limit: {path.name}')
    with path.open('rb') as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise AssetError(f'Local input exceeds the {limit}-byte size limit: {path.name}')
    return data


def sound_basenames(name: str) -> tuple[str, ...]:
    return tuple(f'{prefix}{name}.{suffix}' for prefix in ('', 'sfx_') for suffix in ('ogg', 'wav'))


def load_apk(path: Path) -> tuple[bytes, bytes, dict[str, tuple[str, bytes]]]:
    """Read selected entries into memory. Archive paths stay off the filesystem."""
    if not path.is_file():
        raise AssetError(f'Local APK is not a file: {path}')
    if path.stat().st_size > MAX_APK_BYTES:
        raise AssetError('APK exceeds the 256 MiB input limit')
    try:
        with zipfile.ZipFile(path) as archive:
            entries = archive.infolist()
            if len(entries) > MAX_APK_ENTRIES:
                raise AssetError('APK has too many entries')
            if sum(entry.file_size for entry in entries) > MAX_APK_BYTES:
                raise AssetError('APK exceeds the 256 MiB uncompressed size limit')
            seen = set()
            for entry in entries:
                name = entry.filename
                parts = PurePosixPath(name).parts
                if (not name or '\\' in name or name.startswith('/') or '..' in parts
                        or (parts and ':' in parts[0])):
                    raise AssetError('APK contains an unsafe entry path')
                if name in seen:
                    raise AssetError(f'APK contains a duplicate entry: {name}')
                seen.add(name)
                if stat.S_ISLNK(entry.external_attr >> 16):
                    raise AssetError('APK contains a symbolic link')

            def select(basenames, preferred, limit, required):
                matches = [entry for entry in entries if not entry.is_dir()
                           and PurePosixPath(entry.filename).name in basenames]
                standard = [entry for entry in matches if entry.filename in preferred]
                candidates = standard or matches
                if not candidates:
                    if required:
                        raise AssetError(f'Unsupported APK: missing {basenames[0]}')
                    return None
                if len(candidates) != 1:
                    raise AssetError(f'Ambiguous APK asset: {basenames[0]}')
                entry = candidates[0]
                if entry.flag_bits & 1:
                    raise AssetError('Encrypted APK assets are unsupported')
                if entry.file_size > limit:
                    raise AssetError(f'APK asset exceeds its size limit: {basenames[0]}')
                with archive.open(entry) as source:
                    data = source.read(limit + 1)
                if len(data) > limit:
                    raise AssetError(f'APK asset exceeds its size limit: {basenames[0]}')
                return PurePosixPath(entry.filename).name, data

            atlas = select(('atlas.png',), ('assets/atlas.png',), MAX_IMAGE_BYTES, True)
            manifest = select(('atlas.txt',), ('assets/atlas.txt',), MAX_MANIFEST_BYTES, True)
            sounds = {}
            for name in SOUND_NAMES:
                basenames = sound_basenames(name)
                preferred = tuple(f'assets/{folder}{base}' for folder in ('audio/', 'sounds/', '')
                                  for base in basenames)
                sound = select(basenames, preferred, MAX_AUDIO_BYTES, False)
                if sound is not None:
                    sounds[name] = sound
            return atlas[1], manifest[1], sounds
    except (zipfile.BadZipFile, NotImplementedError, RuntimeError, EOFError) as exc:
        raise AssetError(f'Cannot read APK ZIP data: {exc}') from exc


def load_audio_dir(path: Path) -> dict[str, tuple[str, bytes]]:
    if not path.is_dir():
        raise AssetError(f'Audio directory does not exist: {path}')
    sounds = {}
    for name in SOUND_NAMES:
        candidates = [path / base for base in sound_basenames(name) if (path / base).is_file()]
        if len(candidates) > 1:
            raise AssetError(f'Ambiguous audio input for {name}: keep one supported file')
        if candidates:
            sound = candidates[0]
            sounds[name] = (sound.name, read_local(sound, MAX_AUDIO_BYTES))
    return sounds


def load_atlas(atlas_data: bytes, manifest_data: bytes) -> dict[str, Image.Image]:
    try:
        with Image.open(io.BytesIO(atlas_data)) as opened:
            if opened.format != 'PNG':
                raise AssetError('Atlas must be a PNG image')
            if opened.width * opened.height > MAX_IMAGE_PIXELS:
                raise AssetError('Atlas exceeds the 16-megapixel size limit')
            atlas = opened.convert('RGBA')
    except (UnidentifiedImageError, OSError, Image.DecompressionBombError) as exc:
        raise AssetError(f'Cannot decode atlas PNG: {exc}') from exc
    try:
        text = manifest_data.decode('utf-8-sig')
    except UnicodeDecodeError as exc:
        raise AssetError('Atlas manifest must be UTF-8 text') from exc
    entries = {}
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        fields = line.split()
        if len(fields) != 7:
            raise AssetError(f'Manifest line {number}: expected name width height u v uw vh')
        name, width, height, *coordinates = fields
        if name in entries:
            raise AssetError(f'Manifest line {number}: duplicate entry {name}')
        try:
            w, h = int(width), int(height)
            u, v, uw, vh = (float(value) for value in coordinates)
            if not all(math.isfinite(value) for value in (u, v, uw, vh)):
                raise ValueError
            x, y = round(u * atlas.width), round(v * atlas.height)
        except (ValueError, OverflowError) as exc:
            raise AssetError(f'Manifest line {number}: invalid dimensions or coordinates') from exc
        if w <= 0 or h <= 0 or min(u, v) < 0 or max(u, v) >= 1:
            raise AssetError(f'Manifest line {number}: invalid dimensions or coordinates')
        if x + w > atlas.width or y + h > atlas.height:
            raise AssetError(f'Manifest line {number}: crop extends outside atlas')
        if abs(uw * atlas.width - w) > 0.02 or abs(vh * atlas.height - h) > 0.02:
            raise AssetError(f'Manifest line {number}: normalized dimensions do not match pixel dimensions')
        entries[name] = atlas.crop((x, y, x + w, y + h))
    missing = sorted(REQUIRED_SIZES.keys() - entries.keys())
    if missing:
        raise AssetError('Unsupported atlas: missing required entries: ' + ', '.join(missing))
    for name, size in REQUIRED_SIZES.items():
        if entries[name].size != size:
            raise AssetError(f'Unsupported atlas: {name} must be {size[0]}x{size[1]} pixels')
    return entries


def half_grid(image: Image.Image) -> Image.Image:
    if image.width % 2 or image.height % 2:
        raise AssetError('Sprite dimensions must be even for the exact 2x2 pixel grid')
    def visible(pixel):
        return pixel if pixel[3] else (0, 0, 0, 0)
    for y in range(0, image.height, 2):
        for x in range(0, image.width, 2):
            expected = visible(image.getpixel((x, y)))
            if any(visible(image.getpixel((x + dx, y + dy))) != expected
                   for dx, dy in ((1, 0), (0, 1), (1, 1))):
                raise AssetError(f'Sprite contains a nonuniform 2x2 block at ({x}, {y})')
    return image.resize((image.width // 2, image.height // 2), Image.Resampling.NEAREST)


def rgb565(pixel) -> int:
    r, g, b, a = pixel
    if a == 0:
        return TRANSPARENT
    value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    if value == TRANSPARENT:
        raise AssetError('Opaque sprite pixel collides with the RGB565 transparency key')
    return value


def make_assets(atlas: dict[str, Image.Image]) -> list[tuple[str, Image.Image]]:
    assets = [
        ('background', atlas['bg_day']), ('ground', atlas['land']),
        ('pipe', atlas['pipe_up']), ('pipeTop', atlas['pipe_down']),
        ('birdUp', atlas['bird0_0'].crop((6, 12, 40, 36))),
        ('birdMid', atlas['bird0_1'].crop((6, 12, 40, 36))),
        ('birdDown', atlas['bird0_2'].crop((6, 12, 40, 36))),
        ('title', atlas['title']), ('getReady', atlas['text_ready']),
        ('tapInstruction', atlas['tutorial']), ('gameOver', atlas['text_game_over']),
        ('scoreboard', atlas['score_panel']), ('replay', atlas['button_play']),
        ('newBest', atlas['new']),
    ]
    assets.extend((f'digit{i}', atlas[f'font_{48 + i:03d}'].crop((0, 4, 16 if i == 1 else 24, 40))) for i in range(10))
    assets.extend((f'smallDigit{i}', atlas[f'number_context_{i:02d}']) for i in range(10))
    assets.extend((f'panelDigit{i}', atlas[f'number_score_{i:02d}']) for i in range(10))
    assets.extend((f'medal{i}', atlas[f'medals_{i}']) for i in range(4))
    return assets


def header_preamble() -> list[str]:
    return [
        '// Generated locally from user-supplied assets. Keep this file private.',
        '#pragma once', '#include <stdint.h>', '#if defined(ARDUINO)',
        '#include <pgmspace.h>', '#else', '#ifndef PROGMEM', '#define PROGMEM',
        '#endif', '#endif',
    ]


def sprite_header(assets: list[tuple[str, Image.Image]]) -> str:
    lines = header_preamble() + [
        'namespace FlappyAssets {', 'static constexpr uint16_t kTransparent = 0xF81F;',
        'struct Sprite {', '  const uint16_t* pixels;', '  uint16_t width, height;',
        '  uint16_t sourceWidth, sourceHeight;', '  const uint8_t* alpha;', '};',
    ]
    for name, image in assets:
        try:
            small = half_grid(image)
            pixels = [small.getpixel((x, y)) for y in range(small.height) for x in range(small.width)]
            values = [rgb565(pixel) for pixel in pixels]
        except AssetError as exc:
            raise AssetError(f'{name}: {exc}') from exc
        alpha = [pixel[3] for pixel in pixels]
        has_alpha = any(value not in (0, 255) for value in alpha)
        lines.append(f'static const uint16_t {name}Pixels[] PROGMEM = {{')
        for i in range(0, len(values), 16):
            lines.append('  ' + ','.join(f'0x{value:04X}' for value in values[i:i + 16]) + ',')
        lines.append('};')
        if has_alpha:
            lines.append(f'static const uint8_t {name}Alpha[] PROGMEM = {{')
            for i in range(0, len(alpha), 32):
                lines.append('  ' + ','.join(str(value) for value in alpha[i:i + 32]) + ',')
            lines.append('};')
        alpha_ref = f'{name}Alpha' if has_alpha else 'nullptr'
        lines.append(f'static const Sprite {name} = {{{name}Pixels,{small.width},{small.height},{image.width},{image.height},{alpha_ref}}};')
    for array, prefix in (('digits', 'digit'), ('smallDigits', 'smallDigit'), ('panelDigits', 'panelDigit')):
        lines.append(f'static const Sprite {array}[] = {{' + ','.join(f'{prefix}{i}' for i in range(10)) + '};')
    lines += [
        'static const Sprite medalBronze = medal3;', 'static const Sprite medalSilver = medal0;',
        'static const Sprite medalGold = medal1;', 'static const Sprite medalPlatinum = medal2;',
        'static const Sprite medals[] = {medalBronze,medalSilver,medalGold,medalPlatinum};',
        '}  // namespace FlappyAssets',
    ]
    return '\n'.join(lines) + '\n'


def collision_masks(assets: list[tuple[str, Image.Image]]) -> list[list[int]]:
    images = dict(assets)
    return [[sum(1 << x for x in range(34) if images[name].getpixel((x, y))[3] > 0)
             for y in range(24)] for name in ('birdUp', 'birdMid', 'birdDown')]


def collision_header(assets: list[tuple[str, Image.Image]]) -> str:
    lines = header_preamble() + ['namespace FlappyCollision {', 'static const uint64_t birdMasks[3][24] = {']
    for frame in collision_masks(assets):
        lines.append('  {')
        for offset in range(0, 24, 4):
            lines.append('    ' + ','.join(f'0x{mask:09X}ULL' for mask in frame[offset:offset + 4]) + ',')
        lines.append('  },')
    lines += ['};', '}  // namespace FlappyCollision']
    return '\n'.join(lines) + '\n'


def sound_header(sounds: dict[str, tuple[str, bytes]]) -> str:
    ffmpeg = shutil.which('ffmpeg') if sounds else None
    if sounds and not ffmpeg:
        raise AssetError('ffmpeg is required when audio is supplied. Omit audio inputs for a silent build.')
    lines = header_preamble() + [
        'namespace FlappySounds {',
        'struct Sound { const int16_t* samples; uint32_t length; uint32_t sampleRate; };',
    ]
    with tempfile.TemporaryDirectory(prefix='m5-audio-') as temp:
        for name in SOUND_NAMES:
            if name not in sounds:
                lines.append(f'static const Sound {name} = {{nullptr,0,16000}};')
                continue
            basename, data = sounds[name]
            path = Path(temp) / basename
            path.write_bytes(data)
            try:
                result = subprocess.run([
                    ffmpeg, '-nostdin', '-v', 'error', '-protocol_whitelist', 'file,pipe',
                    '-f', path.suffix[1:], '-i', str(path), '-ac', '1', '-ar', '16000',
                    '-t', str(MAX_SOUND_SECONDS + 1), '-f', 's16le', '-acodec', 'pcm_s16le', '-',
                ], capture_output=True, timeout=30, check=False)
            except (subprocess.TimeoutExpired, OSError) as exc:
                raise AssetError(f'Audio conversion failed for {name}: {exc}') from exc
            if result.returncode:
                raise AssetError(f'Audio conversion failed for {name}: unsupported or corrupt sound file')
            raw = result.stdout
            if not raw or len(raw) % 2 or len(raw) > MAX_SOUND_SECONDS * 16000 * 2:
                raise AssetError(f'Audio for {name} must contain between one sample and {MAX_SOUND_SECONDS} seconds')
            samples = struct.unpack(f'<{len(raw) // 2}h', raw)
            lines.append(f'static const int16_t {name}Samples[] PROGMEM = {{')
            for offset in range(0, len(samples), 24):
                lines.append('  ' + ','.join(str(value) for value in samples[offset:offset + 24]) + ',')
            lines += ['};', f'static const Sound {name} = {{{name}Samples,{len(samples)},16000}};']
    lines.append('}  // namespace FlappySounds')
    return '\n'.join(lines) + '\n'


def generate(atlas_data: bytes, manifest_data: bytes,
             sounds: dict[str, tuple[str, bytes]], output_dir: Path) -> dict:
    assets = make_assets(load_atlas(atlas_data, manifest_data))
    headers = {
        'generated_assets.h': sprite_header(assets),
        'generated_collision.h': collision_header(assets),
        'generated_sounds.h': sound_header(sounds),
    }
    source, crops = output_dir / 'source', output_dir / 'crops'
    source.mkdir(parents=True, exist_ok=True)
    crops.mkdir(parents=True, exist_ok=True)
    (source / 'atlas.png').write_bytes(atlas_data)
    (source / 'atlas.txt').write_bytes(manifest_data)
    for basename, data in sounds.values():
        (source / basename).write_bytes(data)
    for name, image in assets:
        image.save(crops / f'{name}.png')
    for name, header in headers.items():
        (output_dir / name).write_text(header, encoding='utf-8')
    receipt = {
        'format': 1, 'sprites': len(assets),
        'atlas_sha256': hashlib.sha256(atlas_data).hexdigest(),
        'manifest_sha256': hashlib.sha256(manifest_data).hexdigest(),
        'audio_sha256': {name: hashlib.sha256(data).hexdigest() for name, (_, data) in sounds.items()},
        'silent_sounds': [name for name in SOUND_NAMES if name not in sounds],
    }
    (output_dir / 'import_receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return receipt


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--atlas', type=Path, help='Local PNG atlas, with --manifest')
    inputs.add_argument('--apk', type=Path, help='Local, unencrypted APK containing atlas.png and atlas.txt')
    parser.add_argument('--manifest', type=Path, help='Local text atlas manifest')
    parser.add_argument('--audio-dir', type=Path, help='Optional local wing/point/hit/die/swoosh .ogg or .wav files, with optional sfx_ prefixes')
    parser.add_argument('--output-dir', type=Path, default=DEFAULT_OUTPUT, help='Private output directory (default: .local/assets)')
    args = parser.parse_args(argv)
    if args.atlas and not args.manifest:
        parser.error('--atlas requires --manifest')
    if args.apk and args.manifest:
        parser.error('--manifest cannot be combined with --apk')
    try:
        if args.apk:
            atlas, manifest, sounds = load_apk(args.apk)
        else:
            atlas = read_local(args.atlas, MAX_IMAGE_BYTES)
            manifest = read_local(args.manifest, MAX_MANIFEST_BYTES)
            sounds = {}
        if args.audio_dir:
            sounds.update(load_audio_dir(args.audio_dir))
        receipt = generate(atlas, manifest, sounds, args.output_dir)
    except (AssetError, OSError) as exc:
        parser.exit(2, f'Asset import failed: {exc}\n')
    print(f"Generated {receipt['sprites']} sprites and private headers in {args.output_dir}")
    if receipt['silent_sounds']:
        print('Silent sounds: ' + ', '.join(receipt['silent_sounds']))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
