#!/usr/bin/env python3
"""Convert local custom skin folders into private device sprite packs."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath

from PIL import Image, UnidentifiedImageError

try:
    from . import generate_assets as asset_tools
except ImportError:
    import generate_assets as asset_tools

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / '.local' / 'skins'
MAX_SKINS = 10
MAX_MANIFEST_BYTES = 64 * 1024
NATIVE_SIZES = {
    'background': (144, 256), 'ground': (168, 56),
    'pipe': (26, 160), 'pipeTop': (26, 160),
    'birdUp': (17, 12), 'birdMid': (17, 12), 'birdDown': (17, 12),
}
REQUIRED_BIRDS = ('birdUp', 'birdMid', 'birdDown')


class SkinError(ValueError):
    """A local skin cannot be converted safely."""


def cpp_string(value: str) -> str:
    encoded = []
    for byte in value.encode('utf-8'):
        if byte in (34, 92):
            encoded.append('\\' + chr(byte))
        elif 32 <= byte <= 126:
            encoded.append(chr(byte))
        else:
            encoded.append(f'\\{byte:03o}')
    return '"' + ''.join(encoded) + '"'


def local_image_path(directory: Path, value: object) -> tuple[Path, str]:
    if not isinstance(value, str) or not value or '\\' in value or '\x00' in value:
        raise SkinError('Skin image paths must be relative local filenames')
    parts = PurePosixPath(value)
    if parts.is_absolute() or '..' in parts.parts or ':' in value:
        raise SkinError('Skin image path escapes its folder or refers to a URL')
    resolved = (directory / value).resolve()
    try:
        resolved.relative_to(directory.resolve())
    except ValueError as exc:
        raise SkinError('Skin image path escapes its folder') from exc
    return resolved, str(parts)


def normalized_image(data: bytes, size: tuple[int, int], name: str) -> Image.Image:
    try:
        with Image.open(io.BytesIO(data)) as source:
            if source.width * source.height > asset_tools.MAX_IMAGE_PIXELS:
                raise SkinError(f'{name}: image exceeds the 16-megapixel size limit')
            rgba = source.convert('RGBA')
            return rgba.resize(size, Image.Resampling.NEAREST)
    except (UnidentifiedImageError, OSError, Image.DecompressionBombError) as exc:
        raise SkinError(f'{name}: cannot decode skin image') from exc


def load_skin(directory: Path) -> dict:
    manifest_path = directory / 'skin.json'
    manifest_data = asset_tools.read_local(manifest_path, MAX_MANIFEST_BYTES)
    try:
        manifest = json.loads(manifest_data.decode('utf-8-sig'))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SkinError('skin.json must contain a UTF-8 JSON object') from exc
    if not isinstance(manifest, dict):
        raise SkinError('skin.json must contain a JSON object')
    name, files = manifest.get('name'), manifest.get('files')
    if (not isinstance(name, str) or not name.strip() or len(name.encode('utf-8')) > 64
            or any(ord(char) < 32 or ord(char) == 127 for char in name)):
        raise SkinError('Skin name must contain 1 to 64 UTF-8 bytes without control characters')
    if not isinstance(files, dict):
        raise SkinError('skin.json requires a files object')
    missing = [key for key in REQUIRED_BIRDS if key not in files]
    if missing:
        raise SkinError('Skin requires three bird frames: missing ' + ', '.join(missing))
    unknown = set(files) - NATIVE_SIZES.keys()
    if unknown:
        raise SkinError('Unsupported skin image keys: ' + ', '.join(sorted(unknown)))
    metadata = {}
    for field in ('credit', 'license', 'source_url'):
        value = manifest.get(field, '')
        if not isinstance(value, str) or len(value) > 4096:
            raise SkinError(f'Skin {field} must be text with at most 4096 characters')
        metadata[field] = value
    images, inputs = {}, {}
    for key, size in NATIVE_SIZES.items():
        if key not in files:
            continue
        path, relative = local_image_path(directory, files[key])
        data = asset_tools.read_local(path, asset_tools.MAX_IMAGE_BYTES)
        images[key] = normalized_image(data, size, key)
        inputs[relative] = data
    expanded = [(key, images[key].resize((34, 24), Image.Resampling.NEAREST)) for key in REQUIRED_BIRDS]
    masks = asset_tools.collision_masks(expanded)
    return {'name': name.strip(), 'metadata': metadata, 'manifest': manifest_data,
            'inputs': inputs, 'images': images, 'masks': masks}


def sprite_lines(name: str, image: Image.Image) -> tuple[list[str], int]:
    pixels = [image.getpixel((x, y)) for y in range(image.height) for x in range(image.width)]
    try:
        values = [asset_tools.rgb565(pixel) for pixel in pixels]
    except asset_tools.AssetError as exc:
        raise SkinError(f'{name}: {exc}') from exc
    alpha = [pixel[3] for pixel in pixels]
    has_alpha = any(value not in (0, 255) for value in alpha)
    lines = [f'static const uint16_t {name}Pixels[] PROGMEM = {{']
    for offset in range(0, len(values), 16):
        lines.append('  ' + ','.join(f'0x{value:04X}' for value in values[offset:offset + 16]) + ',')
    lines.append('};')
    if has_alpha:
        lines.append(f'static const uint8_t {name}Alpha[] PROGMEM = {{')
        for offset in range(0, len(alpha), 32):
            lines.append('  ' + ','.join(str(value) for value in alpha[offset:offset + 32]) + ',')
        lines.append('};')
    alpha_ref = f'{name}Alpha' if has_alpha else 'nullptr'
    lines.append(f'static const FlappyAssets::Sprite {name} = {{{name}Pixels,{image.width},{image.height},'
                 f'{image.width * 2},{image.height * 2},{alpha_ref}}};')
    return lines, len(values) * 2 + (len(alpha) if has_alpha else 0)


def skin_header(skins: list[dict]) -> tuple[str, int]:
    lines = asset_tools.header_preamble() + [
        '#include "generated_assets.h"', 'namespace FlappySkins {',
        'struct Pack {', '  const char* name;',
        '  const FlappyAssets::Sprite *background, *ground, *pipe, *pipeTop, *birdUp, *birdMid, *birdDown;',
        '  const uint64_t (*birdMasks)[24];', '};',
    ]
    total = 0
    for index, skin in enumerate(skins):
        prefix = f'skin{index}'
        for key, image in skin['images'].items():
            sprite, size = sprite_lines(f'{prefix}_{key}', image)
            lines.extend(sprite)
            total += size
        lines.append(f'static const uint64_t {prefix}_birdMasks[3][24] = {{')
        for frame in skin['masks']:
            lines.append('  {')
            for offset in range(0, 24, 4):
                lines.append('    ' + ','.join(f'0x{mask:09X}ULL' for mask in frame[offset:offset + 4]) + ',')
            lines.append('  },')
        lines.append('};')
        total += 3 * 24 * 8
    if skins:
        lines.append('static const Pack packs[] = {')
        for index, skin in enumerate(skins):
            sprites = [f'&skin{index}_{key}' if key in skin['images'] else 'nullptr' for key in NATIVE_SIZES]
            lines.append('  {' + ','.join([cpp_string(skin['name'])] + sprites + [f'skin{index}_birdMasks']) + '},')
        lines.append('};')
    else:
        lines.append('static const Pack packs[1] = {{nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr}};')
    lines += [f'static const uint8_t count = {len(skins)};', '}  // namespace FlappySkins']
    return '\n'.join(lines) + '\n', total


def generate(skin_dirs: list[Path], output_dir: Path, *, rights_acknowledged: bool = False) -> dict:
    if len(skin_dirs) > MAX_SKINS:
        raise SkinError(f'At most {MAX_SKINS} custom skins can be included in one build')
    skins = [load_skin(directory) for directory in skin_dirs]
    header, byte_count = skin_header(skins)
    receipt = {'format': 1, 'rightsAcknowledged': bool(rights_acknowledged),
               'count': len(skins), 'sprite_and_mask_bytes': byte_count, 'packs': []}
    output_dir.mkdir(parents=True, exist_ok=True)
    for index, skin in enumerate(skins):
        key = f'pack{index + 1:02d}'
        source = output_dir / 'source' / key
        normalized = output_dir / 'normalized' / key
        source.mkdir(parents=True, exist_ok=True)
        normalized.mkdir(parents=True, exist_ok=True)
        (source / 'skin.json').write_bytes(skin['manifest'])
        for relative, data in skin['inputs'].items():
            target = source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        for name, image in skin['images'].items():
            image.save(normalized / f'{name}.png')
        receipt['packs'].append({
            'name': skin['name'], **skin['metadata'],
            'manifest_sha256': hashlib.sha256(skin['manifest']).hexdigest(),
            'input_sha256': {name: hashlib.sha256(data).hexdigest() for name, data in skin['inputs'].items()},
        })
    (output_dir / 'generated_skins.h').write_text(header, encoding='utf-8')
    (output_dir / 'import_receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return receipt


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skin-dir', type=Path, action='append', default=[], help='Local folder containing skin.json, repeat up to 10 times')
    parser.add_argument('--output-dir', type=Path, default=DEFAULT_OUTPUT, help='Private output directory (default: .local/skins)')
    parser.add_argument('--acknowledge-rights', action='store_true', help=asset_tools.RIGHTS_CONFIRMATION)
    args = parser.parse_args(argv)
    if args.skin_dir and not args.acknowledge_rights:
        parser.error('--acknowledge-rights is required when importing custom skins: ' + asset_tools.RIGHTS_CONFIRMATION)
    if len(args.skin_dir) > MAX_SKINS:
        parser.error(f'At most {MAX_SKINS} --skin-dir arguments are supported')
    try:
        receipt = generate(args.skin_dir, args.output_dir, rights_acknowledged=args.acknowledge_rights)
    except (SkinError, asset_tools.AssetError, OSError) as exc:
        parser.exit(2, f'Skin import failed: {exc}\n')
    print(f"Generated {receipt['count']} custom skins in {args.output_dir} ({receipt['sprite_and_mask_bytes']} bytes of sprites and masks)")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
