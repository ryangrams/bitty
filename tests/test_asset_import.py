"""Exercise imports with geometric test images and synthesized audio only."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import wave
import zipfile

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('asset_import', ROOT / 'tools' / 'generate_assets.py')
assets = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets)


def synthetic_atlas():
    """Build independent colored rectangles in the renderer's documented layout."""
    atlas = Image.new('RGBA', (512, 4096), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    manifest, positions = [], {}
    y = 0
    for index, (name, (width, height)) in enumerate(assets.REQUIRED_SIZES.items()):
        positions[name] = (0, y)
        color = (32 + index % 5 * 16, 96, 192, 128 if name == 'title' else 255)
        if name.startswith('bird0_'):
            frame = int(name[-1])
            if frame == 2:
                color = (*color[:3], 128)
            draw.rectangle((6 + frame * 4, y + 12, 9 + frame * 4, y + 15), fill=color)
            draw.rectangle((38, y + 34, 39, y + 35), fill=color)
        else:
            draw.rectangle((0, y, width - 1, y + height - 1), fill=color)
        manifest.append(f'{name} {width} {height} 0 {y / atlas.height} {width / atlas.width} {height / atlas.height}')
        y += height
    # A second color on one 2x2 cell verifies nearest-neighbor sampling.
    draw.rectangle((2, 0, 3, 1), fill=(224, 32, 16, 255))
    buffer = io.BytesIO()
    atlas.save(buffer, format='PNG')
    return buffer.getvalue(), ('\n'.join(manifest) + '\n').encode(), positions


def synthetic_wav():
    buffer = io.BytesIO()
    with wave.open(buffer, 'wb') as sound:
        sound.setnchannels(1)
        sound.setsampwidth(2)
        sound.setframerate(16000)
        sound.writeframes(struct.pack('<8h', 0, 100, 200, 100, 0, -100, -200, -100))
    return buffer.getvalue()


def make_fixture(directory: Path) -> tuple[Path, Path]:
    """Write a geometric atlas and manifest for isolated build and render checks."""
    directory.mkdir(parents=True, exist_ok=True)
    atlas, manifest, _ = synthetic_atlas()
    atlas_path, manifest_path = directory / 'atlas.png', directory / 'atlas.txt'
    atlas_path.write_bytes(atlas)
    manifest_path.write_bytes(manifest)
    return atlas_path, manifest_path


class AssetImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.atlas_data, cls.manifest_data, cls.positions = synthetic_atlas()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='m5-import-test-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def write_apk(self, entries, name='fixture.apk'):
        path = self.directory / name
        with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for basename, data in entries:
                archive.writestr(basename, data)
        return path

    def apk_entries(self):
        return [('assets/atlas.png', self.atlas_data), ('assets/atlas.txt', self.manifest_data)]

    def test_half_grid_preserves_exact_pixels_and_transparent_rgb(self):
        image = Image.new('RGBA', (4, 2), (48, 96, 192, 255))
        for x in (2, 3):
            for y in (0, 1):
                image.putpixel((x, y), (x, y, 200, 0))
        small = assets.half_grid(image)
        self.assertEqual(small.size, (2, 1))
        self.assertEqual(small.getpixel((0, 0)), (48, 96, 192, 255))
        self.assertEqual(assets.rgb565(small.getpixel((1, 0))), assets.TRANSPARENT)

    def test_nonuniform_grid_and_opaque_transparency_key_are_rejected(self):
        image = Image.new('RGBA', (2, 2), (0, 0, 0, 0))
        image.putpixel((1, 1), (255, 0, 0, 255))
        with self.assertRaisesRegex(assets.AssetError, 'nonuniform 2x2 block'):
            assets.half_grid(image)
        with self.assertRaisesRegex(assets.AssetError, 'dimensions must be even'):
            assets.half_grid(Image.new('RGBA', (3, 2)))
        with self.assertRaisesRegex(assets.AssetError, 'transparency key'):
            assets.rgb565((255, 0, 255, 255))

    def test_generate_private_headers_crops_inputs_and_silent_sounds(self):
        output = self.directory / 'private'
        with mock.patch.object(assets.shutil, 'which', return_value=None):
            receipt = assets.generate(self.atlas_data, self.manifest_data, {}, output)
        self.assertEqual(receipt['sprites'], 48)
        self.assertEqual((output / 'source' / 'atlas.png').read_bytes(), self.atlas_data)
        self.assertEqual((output / 'source' / 'atlas.txt').read_bytes(), self.manifest_data)
        self.assertEqual(len(list((output / 'crops').glob('*.png'))), 48)
        self.assertEqual(json.loads((output / 'import_receipt.json').read_text()), receipt)
        self.assertEqual(receipt['silent_sounds'], list(assets.SOUND_NAMES))
        self.assertFalse(receipt['rightsAcknowledged'])
        header = (output / 'generated_assets.h').read_text()
        self.assertIn('backgroundPixels,144,256,288,512,nullptr', header)
        self.assertIn('titleAlpha[]', header)
        self.assertIn('birdUpPixels,17,12,34,24,nullptr', header)
        first = re.search(r'backgroundPixels\[\].*?\{\s*(0x\w+),(0x\w+)', header, re.S)
        self.assertEqual(int(first[1], 16), assets.rgb565((32, 96, 192, 255)))
        self.assertEqual(int(first[2], 16), assets.rgb565((224, 32, 16, 255)))
        silent = (output / 'generated_sounds.h').read_text()
        for name in assets.SOUND_NAMES:
            self.assertIn(f'Sound {name} = {{nullptr,0,16000}}', silent)
        self.assertIn('birdMasks[3][24]', (output / 'generated_collision.h').read_text())

    def test_collision_masks_follow_imported_alpha_for_every_frame(self):
        crops = assets.make_assets(assets.load_atlas(self.atlas_data, self.manifest_data))
        masks = assets.collision_masks(crops)
        for frame in range(3):
            self.assertEqual(masks[frame][:4], [0b1111 << (frame * 4)] * 4)
            self.assertEqual(masks[frame][4:22], [0] * 18)
            self.assertEqual(masks[frame][22:], [0b11 << 32] * 2)
        headers = assets.collision_header(crops)
        values = [int(value, 16) for value in re.findall(r'0x([0-9A-F]+)ULL', headers)]
        self.assertEqual(values, [value for frame in masks for value in frame])

    def test_manifest_diagnostics_and_no_partial_output(self):
        malformed = self.manifest_data.replace(b'bg_day 288', b'bg_day 286', 1)
        cases = [
            (b'invalid\n', 'expected name width'),
            (b'', 'missing required entries'),
            (self.manifest_data + self.manifest_data.splitlines()[0] + b'\n', 'duplicate entry'),
            (self.manifest_data.replace(b' 0 0.0 ', b' nan 0.0 ', 1), 'invalid dimensions'),
            (malformed, 'normalized dimensions'),
            (self.manifest_data.replace(b' 0 0.0 ', b' 0.9 0.0 ', 1), 'outside atlas'),
        ]
        for manifest, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(assets.AssetError, message):
                    assets.generate(self.atlas_data, manifest, {}, self.directory / 'output')
                self.assertFalse((self.directory / 'output').exists())
        with self.assertRaisesRegex(assets.AssetError, 'Cannot decode atlas'):
            assets.load_atlas(b'not an image', self.manifest_data)
        with self.assertRaisesRegex(assets.AssetError, 'must be UTF-8'):
            assets.load_atlas(self.atlas_data, b'\xff')

    def test_apk_reads_standard_entries_and_optional_synthesized_sound(self):
        path = self.write_apk(self.apk_entries() + [('assets/sounds/sfx_wing.wav', synthetic_wav()),
                                                  ('assets/other.bin', b'unused')])
        atlas, manifest, sounds = assets.load_apk(path)
        self.assertEqual(atlas, self.atlas_data)
        self.assertEqual(manifest, self.manifest_data)
        self.assertEqual(sounds, {'wing': ('sfx_wing.wav', synthetic_wav())})
        self.assertEqual(sorted(path.name for path in self.directory.iterdir()), ['fixture.apk'])

    def test_apk_prefers_exact_standard_path_over_unrelated_basename(self):
        path = self.write_apk(self.apk_entries() + [('unrelated/atlas.png', b'unused')])
        self.assertEqual(assets.load_apk(path)[0], self.atlas_data)

    def test_apk_rejects_ambiguous_fallback_and_ambiguous_audio(self):
        cases = [
            ([('one/atlas.png', self.atlas_data), ('two/atlas.png', self.atlas_data),
              ('assets/atlas.txt', self.manifest_data)], 'Ambiguous APK asset: atlas.png'),
            (self.apk_entries() + [('assets/sounds/wing.wav', synthetic_wav()),
                                   ('assets/audio/sfx_wing.wav', synthetic_wav())], 'Ambiguous APK asset: wing.ogg'),
            ([('assets/atlas.png', self.atlas_data)], 'missing atlas.txt'),
        ]
        for entries, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(assets.AssetError, message):
                    assets.load_apk(self.write_apk(entries))

    def test_apk_rejects_unsafe_paths_and_symbolic_links(self):
        for name in ('../atlas.png', '/atlas.png', 'assets\\atlas.png', 'C:/atlas.png'):
            with self.subTest(name=name):
                path = self.write_apk(self.apk_entries() + [(name, b'unsafe')])
                with self.assertRaisesRegex(assets.AssetError, 'unsafe entry path'):
                    assets.load_apk(path)
        link = zipfile.ZipInfo('assets/link')
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        path = self.write_apk(self.apk_entries() + [(link, b'../outside')])
        with self.assertRaisesRegex(assets.AssetError, 'symbolic link'):
            assets.load_apk(path)

    def test_apk_rejects_entry_total_and_member_size_limits(self):
        path = self.write_apk(self.apk_entries())
        with mock.patch.object(assets, 'MAX_APK_ENTRIES', 1):
            with self.assertRaisesRegex(assets.AssetError, 'too many entries'):
                assets.load_apk(path)
        with mock.patch.object(assets, 'MAX_IMAGE_BYTES', 1):
            with self.assertRaisesRegex(assets.AssetError, 'size limit'):
                assets.load_apk(path)
        path = self.write_apk(self.apk_entries() + [('padding', b'x' * 100000)])
        with mock.patch.object(assets, 'MAX_APK_BYTES', path.stat().st_size + 1):
            with self.assertRaisesRegex(assets.AssetError, 'uncompressed size limit'):
                assets.load_apk(path)

    def test_apk_rejects_encrypted_entry_without_attempting_decryption(self):
        path = self.write_apk(self.apk_entries())
        data = bytearray(path.read_bytes())
        # ZIP general-purpose flags are two bytes at offsets 6 and 8.
        for signature, offset in ((b'PK\x03\x04', 6), (b'PK\x01\x02', 8)):
            position = data.find(signature)
            flags = struct.unpack_from('<H', data, position + offset)[0]
            struct.pack_into('<H', data, position + offset, flags | 1)
        path.write_bytes(data)
        with self.assertRaisesRegex(assets.AssetError, 'Encrypted APK assets'):
            assets.load_apk(path)

    def test_local_audio_aliases_missing_files_and_ambiguity(self):
        (self.directory / 'sfx_point.wav').write_bytes(synthetic_wav())
        self.assertEqual(list(assets.load_audio_dir(self.directory)), ['point'])
        (self.directory / 'point.ogg').write_bytes(b'placeholder')
        with self.assertRaisesRegex(assets.AssetError, 'Ambiguous audio input for point'):
            assets.load_audio_dir(self.directory)
        with self.assertRaisesRegex(assets.AssetError, 'not a file'):
            assets.read_local(self.directory / 'missing.png', 10)
        with self.assertRaisesRegex(assets.AssetError, 'size limit'):
            assets.read_local(self.directory / 'point.ogg', 1)

    def test_audio_requires_ffmpeg_only_when_audio_is_supplied(self):
        with mock.patch.object(assets.shutil, 'which', return_value=None):
            self.assertIn('nullptr,0,16000', assets.sound_header({}))
            with self.assertRaisesRegex(assets.AssetError, 'ffmpeg is required'):
                assets.sound_header({'wing': ('wing.wav', synthetic_wav())})

    @unittest.skipUnless(shutil.which('ffmpeg'), 'ffmpeg is optional')
    def test_synthesized_audio_converts_to_pcm_and_bad_audio_reports_error(self):
        header = assets.sound_header({'wing': ('wing.wav', synthetic_wav())})
        self.assertIn('wingSamples,8,16000', header)
        self.assertIn('0,100,200,100,0,-100,-200,-100', header)
        with self.assertRaisesRegex(assets.AssetError, 'unsupported or corrupt sound file'):
            assets.sound_header({'wing': ('wing.wav', b'invalid')})

    def test_cli_requires_explicit_input_and_generates_to_requested_output(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                assets.main([])
        self.assertEqual(raised.exception.code, 2)
        path = self.write_apk(self.apk_entries())
        output = self.directory / 'local-output'
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                assets.main(['--apk', str(path), '--output-dir', str(output)])
        self.assertEqual(raised.exception.code, 2)
        self.assertFalse(output.exists())
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(assets.main(['--apk', str(path), '--output-dir', str(output),
                                         '--acknowledge-rights']), 0)
        self.assertTrue((output / 'generated_collision.h').is_file())
        receipt = json.loads((output / 'import_receipt.json').read_text())
        self.assertTrue(receipt['rightsAcknowledged'])
        self.assertEqual(assets.DEFAULT_OUTPUT, ROOT / '.local' / 'assets')

    def test_generated_headers_compile_as_standard_cpp(self):
        compiler = shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler is unavailable')
        output = self.directory / 'headers'
        assets.generate(self.atlas_data, self.manifest_data, {}, output)
        source = output / 'fixture.cpp'
        source.write_text('#include "generated_assets.h"\n#include "generated_sounds.h"\n#include "generated_collision.h"\n'
                          'int main() { return FlappyAssets::birdUp.width != 17 || FlappySounds::wing.length != 0 || '
                          'FlappyCollision::birdMasks[0][0] != 15; }\n')
        result = subprocess.run([compiler, '-std=c++11', '-Wall', '-Wextra', '-pedantic-errors', str(source),
                                 '-o', str(output / 'fixture')], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run([str(output / 'fixture')], check=False).returncode, 0)


if __name__ == '__main__':
    unittest.main()
