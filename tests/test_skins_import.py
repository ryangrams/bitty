"""Check custom skins with independently drawn geometric fixtures."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(f'tools.{name}', ROOT / 'tools' / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


skins = load_tool('generate_skins')
gui = load_tool('import_gui')


def make_skin(directory: Path, worlds: bool = False):
    directory.mkdir(parents=True, exist_ok=True)
    manifest = {'name': 'Geometric fixture', 'files': {}, 'credit': 'Test fixture',
                'license': 'Fixture data', 'source_url': ''}
    for frame, name in enumerate(('birdUp', 'birdMid', 'birdDown')):
        image = Image.new('RGBA', (34, 24), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        draw.rectangle((frame * 4, 0, frame * 4 + 3, 3), fill=(48, 112, 192, 128 if frame == 0 else 255))
        draw.rectangle((32, 22, 33, 23), fill=(192, 64, 32, 255))
        image.save(directory / f'{name}.png')
        manifest['files'][name] = f'{name}.png'
    if worlds:
        for name in ('background', 'ground', 'pipe', 'pipeTop'):
            image = Image.new('RGBA', (4, 4), (32, 144, 208, 255))
            ImageDraw.Draw(image).rectangle((2, 0, 3, 3), fill=(208, 80, 48, 255))
            image.save(directory / f'{name}.png')
            manifest['files'][name] = f'{name}.png'
    (directory / 'skin.json').write_text(json.dumps(manifest))
    return manifest


class SkinImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='m5-skin-test-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.skin_dir = self.directory / 'input'
        self.output = self.directory / 'private'

    def update_manifest(self, manifest):
        (self.skin_dir / 'skin.json').write_text(json.dumps(manifest))

    def test_empty_pack_requires_no_acknowledgment(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(skins.main(['--output-dir', str(self.output)]), 0)
        header = (self.output / 'generated_skins.h').read_text()
        self.assertIn('Pack packs[1]', header)
        self.assertIn('uint8_t count = 0', header)
        self.assertIn('#include "generated_assets.h"', header)
        receipt = json.loads((self.output / 'import_receipt.json').read_text())
        self.assertEqual(receipt['count'], 0)
        self.assertFalse(receipt['rightsAcknowledged'])
        self.assertFalse((self.output / 'source').exists())

    def test_no_acknowledgment_fails_before_reads_or_outputs(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                skins.main(['--skin-dir', str(self.skin_dir), '--output-dir', str(self.output)])
        self.assertEqual(raised.exception.code, 2)
        self.assertFalse(self.output.exists())

    def test_local_skin_normalizes_worlds_birds_and_alpha(self):
        make_skin(self.skin_dir, worlds=True)
        loaded = skins.load_skin(self.skin_dir)
        for name, dimensions in skins.NATIVE_SIZES.items():
            self.assertEqual(loaded['images'][name].size, dimensions)
        background = loaded['images']['background']
        self.assertEqual(background.getpixel((0, 0)), (32, 144, 208, 255))
        self.assertEqual(background.getpixel((143, 255)), (208, 80, 48, 255))
        self.assertEqual(loaded['images']['birdUp'].getpixel((0, 0))[3], 128)
        for frame in range(3):
            self.assertEqual(loaded['masks'][frame][:4], [15 << (frame * 4)] * 4)
            self.assertEqual(loaded['masks'][frame][4:22], [0] * 18)
            self.assertEqual(loaded['masks'][frame][22:], [3 << 32] * 2)
        receipt = skins.generate([self.skin_dir], self.output)
        self.assertFalse(receipt['rightsAcknowledged'])
        self.assertEqual(receipt['count'], 1)
        self.assertGreater(receipt['sprite_and_mask_bytes'], 100000)
        self.assertTrue((self.output / 'source' / 'pack01' / 'skin.json').is_file())
        self.assertEqual(len(list((self.output / 'normalized' / 'pack01').glob('*.png'))), 7)
        self.assertEqual(receipt['packs'][0]['credit'], 'Test fixture')
        self.assertEqual(receipt['packs'][0]['license'], 'Fixture data')

    def test_three_birds_are_required_and_worlds_can_use_base_assets(self):
        manifest = make_skin(self.skin_dir)
        loaded = skins.load_skin(self.skin_dir)
        header, _ = skins.skin_header([loaded])
        self.assertIn('"Geometric fixture",nullptr,nullptr,nullptr,nullptr,&skin0_birdUp', header)
        del manifest['files']['birdDown']
        self.update_manifest(manifest)
        with self.assertRaisesRegex(skins.SkinError, 'missing birdDown'):
            skins.generate([self.skin_dir], self.output)
        self.assertFalse(self.output.exists())

    def test_invalid_names_and_manifest_values_have_diagnostics(self):
        original = make_skin(self.skin_dir)
        for name in ('', ' ', 'x' * 65, 'bad\x00name'):
            manifest = dict(original, name=name)
            self.update_manifest(manifest)
            with self.assertRaisesRegex(skins.SkinError, 'Skin name'):
                skins.load_skin(self.skin_dir)
        manifest = dict(original, files=dict(original['files'], typo='birdUp.png'))
        self.update_manifest(manifest)
        with self.assertRaisesRegex(skins.SkinError, 'Unsupported skin image keys: typo'):
            skins.load_skin(self.skin_dir)
        (self.skin_dir / 'skin.json').write_text('{')
        with self.assertRaisesRegex(skins.SkinError, 'UTF-8 JSON object'):
            skins.load_skin(self.skin_dir)

    def test_local_image_paths_reject_escape_urls_and_symlink_escape(self):
        original = make_skin(self.skin_dir)
        for value in ('../bird.png', '/tmp/bird.png', 'https://invalid.example/bird.png',
                      'folder\\bird.png', 'C:/bird.png'):
            with self.subTest(value=value):
                manifest = dict(original, files=dict(original['files'], birdUp=value))
                self.update_manifest(manifest)
                with self.assertRaisesRegex(skins.SkinError, 'path|filename'):
                    skins.load_skin(self.skin_dir)
        outside = self.directory / 'outside.png'
        Image.new('RGBA', (2, 2), (0, 128, 0, 255)).save(outside)
        (self.skin_dir / 'link.png').symlink_to(outside)
        self.update_manifest(dict(original, files=dict(original['files'], birdUp='link.png')))
        with self.assertRaisesRegex(skins.SkinError, 'escapes its folder'):
            skins.load_skin(self.skin_dir)

    def test_relative_subfolders_preserve_private_sources(self):
        manifest = make_skin(self.skin_dir)
        (self.skin_dir / 'images').mkdir()
        (self.skin_dir / 'birdUp.png').rename(self.skin_dir / 'images' / 'up.png')
        manifest['files']['birdUp'] = 'images/up.png'
        self.update_manifest(manifest)
        skins.generate([self.skin_dir], self.output)
        self.assertTrue((self.output / 'source' / 'pack01' / 'images' / 'up.png').is_file())

    def test_cli_receipt_records_explicit_acknowledgment(self):
        make_skin(self.skin_dir)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(skins.main(['--skin-dir', str(self.skin_dir), '--acknowledge-rights',
                                         '--output-dir', str(self.output)]), 0)
        receipt = json.loads((self.output / 'import_receipt.json').read_text())
        self.assertTrue(receipt['rightsAcknowledged'])

    def test_ten_skins_allowed_and_eleven_rejected_without_output(self):
        make_skin(self.skin_dir)
        receipt = skins.generate([self.skin_dir] * 10, self.output)
        self.assertEqual(receipt['count'], 10)
        with self.assertRaisesRegex(skins.SkinError, 'At most 10'):
            skins.generate([self.skin_dir] * 11, self.directory / 'too-many')
        self.assertFalse((self.directory / 'too-many').exists())

    def compile_header(self, contents, populated):
        compiler = shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler is unavailable')
        self.output.mkdir(parents=True, exist_ok=True)
        (self.output / 'generated_skins.h').write_text(contents)
        (self.output / 'generated_assets.h').write_text(
            '#pragma once\n#include <stdint.h>\nnamespace FlappyAssets { struct Sprite { '
            'const uint16_t* pixels; uint16_t width,height,sourceWidth,sourceHeight; const uint8_t* alpha; }; }\n')
        test = ('FlappySkins::count != 1 || FlappySkins::packs[0].background != nullptr || '
                'FlappySkins::packs[0].birdUp->width != 17 || FlappySkins::packs[0].birdMasks[0][0] != 15'
                if populated else 'FlappySkins::count != 0')
        source = self.output / 'check.cpp'
        source.write_text('#include "generated_skins.h"\nint main() { return ' + test + '; }\n')
        result = subprocess.run([compiler, '-std=c++11', '-Wall', '-Wextra', '-pedantic-errors', str(source),
                                 '-o', str(self.output / 'check')], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run([str(self.output / 'check')], check=False).returncode, 0)

    def test_empty_header_compiles(self):
        header, size = skins.skin_header([])
        self.assertEqual(size, 0)
        self.compile_header(header, False)

    def test_custom_header_and_utf8_names_compile(self):
        manifest = make_skin(self.skin_dir)
        manifest['name'] = 'Fixture "bird" 🐦'
        self.update_manifest(manifest)
        header, _ = skins.skin_header([skins.load_skin(self.skin_dir)])
        self.compile_header(header, True)

    def test_gui_build_arguments_requires_checkbox_and_local_inputs(self):
        with self.assertRaisesRegex(ValueError, 'rights confirmation'):
            gui.build_arguments('apk', 'local.apk', '', '', '', 'out', False)
        args = gui.build_arguments('apk', 'local.apk', '', '', '', 'out', True)
        self.assertIn('--acknowledge-rights', args)
        self.assertIn('--apk', args)
        self.assertNotIn('--atlas', args)
        args = gui.build_arguments('atlas', '', 'atlas.png', 'atlas.txt', 'audio', 'out', True)
        self.assertEqual(args[-2:], ['--audio-dir', 'audio'])
        with self.assertRaisesRegex(ValueError, 'both a local atlas'):
            gui.build_arguments('atlas', '', 'atlas.png', '', '', 'out', True)

    def test_gui_help_is_available_without_opening_tk(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools' / 'import_gui.py'), '--help'],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('native local-file importer', result.stdout)


if __name__ == '__main__':
    unittest.main()
