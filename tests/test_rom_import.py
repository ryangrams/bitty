"""Check local ROM imports with synthetic cartridge bytes."""
from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import import_rom as importer
import build


def fixture(mapper=0, prg=1, chr=1, *, nes2=False, pal=False):
    header = bytearray(b'NES\x1a' + bytes(12))
    header[4], header[5] = prg & 255, chr & 255
    header[6] = (mapper & 15) << 4
    header[7] = (mapper & 0xf0) | (8 if nes2 else 0)
    if nes2:
        header[8] = (mapper >> 8) & 15
        header[9] = ((prg >> 8) & 15) | (((chr >> 8) & 15) << 4)
        header[12] = int(pal)
    else:
        header[9] = int(pal)
    return bytes(header) + bytes([0xea]) * (prg * 16384) + bytes([0x55]) * (chr * 8192)


def altered(rom, index, value):
    changed = bytearray(rom)
    changed[index] = value
    return bytes(changed)


class RomImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='m5-rom-test-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.output = self.directory / 'private'

    def test_empty_header_requires_no_rights_acknowledgment(self):
        receipt = importer.generate(None, self.output)
        self.assertEqual(receipt['bytes'], 0)
        self.assertIsNone(receipt['rom_sha256'])
        self.assertFalse(receipt['rightsAcknowledged'])
        header = (self.output / 'generated_rom.h').read_text()
        self.assertIn('data = nullptr', header)
        self.assertIn('size_t length = 0', header)
        self.assertIn('title[] = "NES"', header)
        self.assertFalse((self.output / 'source').exists())
        self.assertEqual(json.loads((self.output / 'import_receipt.json').read_text()), receipt)

    def test_acknowledgment_is_required_before_validation_or_output(self):
        with self.assertRaisesRegex(importer.RomError, 'acknowledgment is required'):
            importer.generate(b'invalid', self.output)
        self.assertFalse(self.output.exists())
        with contextlib.redirect_stderr(io.StringIO()), mock.patch.object(importer, 'local_rom_path') as read:
            with self.assertRaises(SystemExit) as raised:
                importer.main(['--rom', 'missing.nes', '--output-dir', str(self.output)])
        self.assertEqual(raised.exception.code, 2)
        read.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_classic_mapper_zero_bank_counts_and_pal(self):
        for prg in (1, 2):
            for chr in (0, 1):
                for pal in (False, True):
                    metadata = importer.inspect_rom(fixture(prg=prg, chr=chr, pal=pal))
                    self.assertEqual(metadata['mapper'], 0)
                    self.assertEqual(metadata['prg_bytes'], prg * 16384)
                    self.assertEqual(metadata['chr_bytes'], chr * 8192)
                    self.assertEqual(metadata['timing'], 'PAL' if pal else 'NTSC')
                    self.assertEqual(metadata['frame_rate'], 50 if pal else 60)

    def test_mapper_37_nes2_and_classic_preserve_whole_cartridge(self):
        for nes2 in (False, True):
            rom = fixture(37, 16, 32, nes2=nes2, pal=True)
            if nes2:
                rom = altered(rom, 15, 2)
            target = self.output / str(nes2)
            receipt = importer.generate(rom, target, 'Local multicart', rights_acknowledged=True)
            self.assertEqual((target / 'source' / 'rom.nes').read_bytes(), rom)
            self.assertEqual(receipt['rom_sha256'], hashlib.sha256(rom).hexdigest())
            self.assertEqual(receipt['bytes'], 524304)
            self.assertEqual(receipt['mapper'], 37)
            self.assertEqual(receipt['prg_bytes'], 262144)
            self.assertEqual(receipt['chr_bytes'], 262144)
            self.assertEqual(receipt['frame_rate'], 50)
            self.assertEqual(receipt['expansion_device'], 2 if nes2 else 0)
            self.assertTrue(receipt['rightsAcknowledged'])
            header = (target / 'generated_rom.h').read_text()
            self.assertIn('alignas(4) static const uint8_t data[] PROGMEM', header)
            self.assertIn('size_t length = sizeof(data)', header)

    def test_mapper_one_accepts_supported_banks_and_preserves_source_and_battery(self):
        for nes2 in (False, True):
            for prg in (1, 2, 4, 8, 16):
                for chr in (0, 1, 2, 4, 8, 16):
                    with self.subTest(nes2=nes2, prg=prg, chr=chr):
                        metadata = importer.inspect_rom(fixture(1, prg, chr, nes2=nes2))
                        self.assertEqual(metadata['mapper'], 1)
                        self.assertEqual(metadata['prg_bytes'], prg * 16384)
                        self.assertEqual(metadata['chr_bytes'], chr * 8192)
        for nes2 in (False, True):
            for battery in (False, True):
                rom = fixture(1, 16, 0, nes2=nes2, pal=True)
                rom = altered(rom, 6, rom[6] | (2 if battery else 0))
                if nes2:
                    rom = altered(rom, 11, 7)
                target = self.output / f'mmc1-{nes2}-{battery}'
                receipt = importer.generate(rom, target, 'Synthetic MMC1', rights_acknowledged=True)
                self.assertEqual((target / 'source' / 'rom.nes').read_bytes(), rom)
                self.assertEqual(receipt['rom_sha256'], hashlib.sha256(rom).hexdigest())
                self.assertEqual(receipt['battery'], battery)
                self.assertEqual(receipt['chr_ram_header'], 7 if nes2 else 0)
                self.assertEqual(receipt['frame_rate'], 50)

    def test_mapper_one_rejects_invalid_banks_lengths_and_ram_declarations(self):
        for nes2 in (False, True):
            for prg, chr in ((0, 0), (3, 0), (32, 0), (1, 3), (1, 32)):
                with self.subTest(nes2=nes2, prg=prg, chr=chr):
                    with self.assertRaisesRegex(importer.RomError, 'Mapper 1'):
                        importer.generate(fixture(1, prg, chr, nes2=nes2), self.output,
                                          rights_acknowledged=True)
                    self.assertFalse(self.output.exists())
        rom = fixture(1, 2, 0, nes2=True)
        for data in (rom[:-1], rom + b'x'):
            with self.assertRaisesRegex(importer.RomError, 'match its header exactly'):
                importer.generate(data, self.output, rights_acknowledged=True)
            self.assertFalse(self.output.exists())
        for index, value in ((10, 7), (10, 0x70), (11, 1), (11, 6), (11, 8), (11, 0x70), (11, 0x77)):
            with self.subTest(index=index, value=value):
                with self.assertRaisesRegex(importer.RomError, 'RAM'):
                    importer.generate(altered(rom, index, value), self.output, rights_acknowledged=True)
                self.assertFalse(self.output.exists())
        with self.assertRaisesRegex(importer.RomError, 'CHR RAM'):
            importer.inspect_rom(altered(fixture(1, 2, 1, nes2=True), 11, 7))

    def test_validation_errors_leave_no_output(self):
        rom = fixture(nes2=True)
        cases = [
            (b'', 'incomplete'), (b'NOPE' + bytes(12), 'header'),
            (rom[:-1], 'match its header exactly'), (rom + b'x', 'match its header exactly'),
            (altered(rom, 6, 4), 'trainers'), (altered(rom, 7, 9), 'console'),
            (altered(rom, 7, 12), 'format'), (altered(rom, 8, 0x10), 'submapper'),
            (altered(rom, 8, 1), 'mapper 256'),
            (altered(rom, 9, 15), 'exponent'), (altered(rom, 9, 0xf0), 'exponent'),
            (altered(rom, 9, 1), '1 MiB'), (altered(rom, 9, 0x10), '1 MiB'),
            (altered(rom, 12, 3), 'Dendy'), (altered(rom, 12, 4), 'reserved'),
            (altered(rom, 13, 1), 'console extensions'), (altered(rom, 14, 1), 'Miscellaneous'),
            (altered(rom, 14, 4), 'reserved'), (altered(rom, 15, 0x40), 'reserved'),
            (altered(rom, 10, 7), 'PRG RAM'), (altered(rom, 10, 0x70), 'NVRAM'),
            (altered(rom, 11, 7), 'CHR RAM'), (altered(rom, 11, 0x70), 'CHR RAM'),
            (fixture(2), 'mapper 2'), (fixture(0, 3), 'Mapper 0'),
            (fixture(0, 1, 2), 'Mapper 0'), (fixture(37, 8, 32), 'Mapper 37'),
            (bytes(importer.MAX_ROM_BYTES + 1), '1 MiB'),
        ]
        for data, message in cases:
            with self.subTest(message=message, size=len(data)):
                with self.assertRaisesRegex(importer.RomError, message):
                    importer.generate(data, self.output, rights_acknowledged=True)
                self.assertFalse(self.output.exists())

    def test_nes2_multiregion_and_only_supported_chr_ram(self):
        rom = fixture(nes2=True, chr=0)
        metadata = importer.inspect_rom(altered(rom, 11, 7))
        self.assertEqual(metadata['chr_ram_header'], 7)
        metadata = importer.inspect_rom(altered(rom, 12, 2))
        self.assertEqual(metadata['timing'], 'Multi-region')
        self.assertEqual(metadata['frame_rate'], 60)
        for shift in (1, 6, 8, 0x77):
            with self.assertRaisesRegex(importer.RomError, 'CHR RAM'):
                importer.inspect_rom(altered(rom, 11, shift))

    def test_classic_reserved_padding_and_ram_bounds(self):
        rom = fixture()
        for index in range(10, 16):
            with self.assertRaisesRegex(importer.RomError, 'reserved'):
                importer.inspect_rom(altered(rom, index, 1))
        with self.assertRaisesRegex(importer.RomError, 'reserved'):
            importer.inspect_rom(altered(rom, 9, 2))
        with self.assertRaisesRegex(importer.RomError, 'PRG RAM'):
            importer.inspect_rom(altered(rom, 8, 2))
        self.assertEqual(importer.inspect_rom(altered(rom, 8, 1))['prg_ram_header'], 1)

    def test_titles_are_utf8_bounded_clean_and_cpp_escaped(self):
        self.assertEqual(importer.clean_title('  NES  '), 'NES')
        self.assertEqual(importer.clean_title('é' * 20), 'é' * 20)
        for value in ('', ' ', 'x' * 41, 'é' * 21, 'bad\x00title', 'bad\ntitle', 'bad\n',
                      '\tbad', '\u200bhidden', '\ud800'):
            with self.subTest(value=repr(value)):
                with self.assertRaises(importer.RomError):
                    importer.generate(None, self.output, value)
                self.assertFalse(self.output.exists())
        title = 'A "quote" \\ é'
        receipt = importer.generate(fixture(), self.output, title, rights_acknowledged=True)
        self.assertEqual(receipt['title'], title)
        header = (self.output / 'generated_rom.h').read_text()
        self.assertIn('\\"quote\\"', header)
        self.assertIn('\\\\', header)
        self.assertIn('\\303\\251', header)

    def test_local_path_and_cli_reject_urls_archives_stdin(self):
        for value in ('https://example.test/game.nes', 'file:///tmp/game.nes', 'data:game.nes',
                      '-', 'game.zip', 'game.nes.zip', ''):
            with self.subTest(value=value), self.assertRaises(importer.RomError):
                importer.local_rom_path(value)
        for value in ('https://example.test/game.nes', ''):
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    importer.main(['--rom', value, '--acknowledge-rights', '--output-dir', str(self.output)])
            self.assertEqual(raised.exception.code, 2)
            self.assertFalse(self.output.exists())

    def test_cli_reads_local_nes_and_records_acknowledgment(self):
        source = self.directory / 'example.NES'
        source.write_bytes(fixture())
        with contextlib.redirect_stdout(io.StringIO()):
            result = importer.main(['--rom', str(source), '--title', 'Synthetic fixture',
                                    '--acknowledge-rights', '--output-dir', str(self.output)])
        self.assertEqual(result, 0)
        self.assertEqual((self.output / 'source' / 'rom.nes').read_bytes(), source.read_bytes())
        receipt = json.loads((self.output / 'import_receipt.json').read_text())
        self.assertTrue(receipt['rightsAcknowledged'])
        self.assertEqual(receipt['title'], 'Synthetic fixture')

    def test_empty_and_real_headers_compile_and_keep_exact_bytes(self):
        compiler = shutil.which('c++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        for index, rom in enumerate((None, fixture())):
            target = self.output / str(index)
            importer.generate(rom, target, 'Synthetic fixture', rights_acknowledged=rom is not None)
            program = target / 'check.cpp'
            program.write_text('#include "generated_rom.h"\n#include <cassert>\nint main() {\n'
                               + ('assert(FlappyRom::data == nullptr && FlappyRom::length == 0);\n' if rom is None else
                                  f'assert(FlappyRom::length == {len(rom)});\n'
                                  'assert(FlappyRom::data[0] == 78 && FlappyRom::data[16] == 0xea);\n')
                               + 'return 0; }\n')
            executable = target / 'check'
            subprocess.run([compiler, '-std=c++11', '-Wall', '-Wextra', '-Werror', str(program),
                            '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def make_build_project(self):
        assets = self.directory / 'assets'
        assets.mkdir()
        for header in build.HEADERS:
            (assets / header).write_text('// synthetic header\n')
        project = self.directory / 'project'
        firmware = project / 'firmware' / 'flappy'
        firmware.mkdir(parents=True)
        (firmware / 'flappy.ino').write_text('// synthetic sketch\n')
        return project, assets

    def test_build_stages_separate_empty_rom_namespaces(self):
        project, assets = self.make_build_project()
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets)
            for filename, namespace, title in (('generated_rom.h', 'FlappyRom', 'NES'),
                                                ('generated_tetris_rom.h', 'TetrisRom', 'Tetris'),
                                                ('generated_zero_rom.h', 'TetrisZeroRom', 'Tetris Zero'),
                                                ('generated_geometry_rom.h', 'GeometryRom', 'Geometry Dash')):
                header = (target / filename).read_text()
                self.assertIn('namespace ' + namespace + ' {', header)
                self.assertIn('data = nullptr', header)
                self.assertIn('size_t length = 0', header)
                self.assertIn(f'title[] = "{title}"', header)
            self.assertTrue((target / 'generated_skins.h').is_file())
            compiler = shutil.which('c++') or shutil.which('g++')
            if not compiler:
                self.skipTest('C++ compiler unavailable')
            program = self.directory / 'check-stubs.cpp'
            program.write_text('#include "generated_rom.h"\n#include "generated_tetris_rom.h"\n'
                               '#include "generated_zero_rom.h"\n#include "generated_geometry_rom.h"\n'
                               '#include <cassert>\n#include <cstring>\nint main() {\n'
                               'assert(FlappyRom::data == nullptr && FlappyRom::length == 0);\n'
                               'assert(TetrisRom::data == nullptr && TetrisRom::length == 0);\n'
                               'assert(TetrisZeroRom::data == nullptr && TetrisZeroRom::length == 0);\n'
                               'assert(GeometryRom::data == nullptr && GeometryRom::length == 0);\n'
                               'assert(std::strcmp(FlappyRom::title, "NES") == 0);\n'
                               'assert(std::strcmp(TetrisRom::title, "Tetris") == 0);\n'
                               'assert(std::strcmp(TetrisZeroRom::title, "Tetris Zero") == 0);\n'
                               'assert(std::strcmp(GeometryRom::title, "Geometry Dash") == 0);\n}\n')
            executable = self.directory / 'check-stubs'
            subprocess.run([compiler, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', str(target),
                            str(program), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def test_build_uses_private_defaults_and_independent_explicit_rom_overrides(self):
        project, assets = self.make_build_project()
        default_mario, default_tetris = project / '.local' / 'nes-rom', project / '.local' / 'tetris-rom'
        explicit_mario, explicit_tetris = self.directory / 'mario', self.directory / 'tetris'
        imports = ((default_mario, fixture(), 'Default Mario'),
                   (default_tetris, fixture(1, 2, 0), 'Default Tetris'),
                   (explicit_mario, fixture(prg=2), 'Explicit Mario'),
                   (explicit_tetris, fixture(1, 4, 0), 'Explicit Tetris'))
        for directory, rom, title in imports:
            importer.generate(rom, directory, title, rights_acknowledged=True)
        with mock.patch.object(build, 'ROOT', project):
            for mario, tetris, expected_mario, expected_tetris in (
                    (None, None, default_mario, default_tetris),
                    (explicit_mario, None, explicit_mario, default_tetris),
                    (None, explicit_tetris, default_mario, explicit_tetris),
                    (explicit_mario, explicit_tetris, explicit_mario, explicit_tetris)):
                with self.subTest(mario=mario, tetris=tetris):
                    target = build.prepare_sketch(assets, None, mario, None, tetris)
                    self.assertEqual((target / 'generated_rom.h').read_bytes(),
                                     (expected_mario / 'generated_rom.h').read_bytes())
                    expected = (expected_tetris / 'generated_rom.h').read_text().replace(
                        'namespace FlappyRom', 'namespace TetrisRom')
                    self.assertEqual((target / 'generated_tetris_rom.h').read_text(), expected)
                    self.assertNotIn('namespace FlappyRom', expected)
                    self.assertNotEqual((target / 'generated_rom.h').read_text(), expected)
        for directory, rom, _ in imports:
            self.assertEqual((directory / 'source' / 'rom.nes').read_bytes(), rom)

    def test_public_build_ignores_private_defaults_and_rejects_explicit_inputs(self):
        project, assets = self.make_build_project()
        for name in ('nes-rom', 'tetris-rom', 'tetris-zero-rom', 'geometry-rom'):
            importer.generate(fixture(), project / '.local' / name, 'PRIVATE INPUT', rights_acknowledged=True)
        private_jetpack = project / '.local/jetpack/imported'
        private_jetpack.mkdir(parents=True)
        (private_jetpack / 'jetpack_imported.h').write_text('// PRIVATE INPUT\n')
        firmware = project / 'firmware/flappy'
        (firmware / 'partitions.csv').write_bytes((ROOT / 'firmware/flappy/partitions.csv').read_bytes())
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets, display='stickc-plus', public=True, library=True)
            for name in ('generated_rom.h', 'generated_tetris_rom.h', 'generated_zero_rom.h', 'generated_geometry_rom.h'):
                self.assertIn('length = 0', (target / name).read_text())
                self.assertNotIn('PRIVATE INPUT', (target / name).read_text())
            self.assertFalse((target / 'jetpack_imported.h').exists())
            self.assertIn('M5FLAP_USB_LIBRARY', (target / 'build_display.h').read_text())
            self.assertTrue((target / 'partitions.csv').exists())
            for option in ('skins', 'rom', 'tetris', 'zero', 'geometry', 'jetpack'):
                with self.subTest(option=option), self.assertRaises(SystemExit):
                    build.prepare_sketch(assets, public=True, **{option: self.directory})

    def test_missing_explicit_rom_or_tetris_preserves_existing_staged_sketch(self):
        project, assets = self.make_build_project()
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets)
            (target / 'preserve.txt').write_text('old sketch')
            staged = {path.name: path.read_bytes() for path in target.iterdir() if path.is_file()}
            for mario, tetris in ((self.directory / 'missing-mario', None),
                                   (None, self.directory / 'missing-tetris')):
                with self.subTest(mario=mario, tetris=tetris):
                    with self.assertRaisesRegex(SystemExit, 'generated_rom.h'):
                        build.prepare_sketch(assets, None, mario, None, tetris)
                    self.assertEqual({path.name: path.read_bytes() for path in target.iterdir() if path.is_file()},
                                     staged)


    def test_display_profile_defaults_persists_and_explicit_auto_resets(self):
        project = self.directory / 'project'
        profile = project / '.local' / 'display-profile.json'
        assets = self.directory / 'display-assets'
        assets.mkdir()
        for header in build.HEADERS:
            (assets / header).write_text('// synthetic header\n')
        firmware = project / 'firmware' / 'flappy'
        firmware.mkdir(parents=True)
        (firmware / 'flappy.ino').write_text('// synthetic sketch\n')
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets)
            self.assertEqual((target / 'build_display.h').read_text(),
                             '#pragma once\n#define M5FLAP_DISPLAY_PROFILE 0\n')
            self.assertFalse(profile.exists())
            for choice, number in (('stickc', 1), ('stickc-plus', 2), ('auto', 0)):
                target = build.prepare_sketch(assets, None, None, choice)
                self.assertEqual(json.loads(profile.read_text()), choice)
                self.assertIn(f'M5FLAP_DISPLAY_PROFILE {number}', (target / 'build_display.h').read_text())
                target = build.prepare_sketch(assets)
                self.assertIn(f'M5FLAP_DISPLAY_PROFILE {number}', (target / 'build_display.h').read_text())
            self.assertEqual(build.resolve_display_profile(), 'auto')

    def test_display_profile_rejects_invalid_or_oversized_cache_and_can_recover(self):
        project = self.directory / 'project'
        profile = project / '.local' / 'display-profile.json'
        profile.parent.mkdir(parents=True)
        invalid = (b'{', b'null', b'2', b'[]', b'{"display":"stickc"}', b'"unknown"',
                   b'"stickc" "auto"', b'"stickc"' + b' ' * 129, b'\xff')
        with mock.patch.object(build, 'ROOT', project):
            for data in invalid:
                profile.write_bytes(data)
                with self.assertRaisesRegex(SystemExit, 'Invalid private display profile'):
                    build.resolve_display_profile()
                self.assertEqual(profile.read_bytes(), data)
            for choice in ('bad', '', 2, []):
                with self.assertRaisesRegex(SystemExit, 'Display profile must be'):
                    build.resolve_display_profile(choice)
            self.assertEqual(build.resolve_display_profile('stickc-plus'), 'stickc-plus')
            self.assertEqual(build.resolve_display_profile(), 'stickc-plus')

    def test_invalid_display_profile_does_not_clear_existing_sketch(self):
        project = self.directory / 'project'
        assets = self.directory / 'display-assets'
        assets.mkdir()
        for header in build.HEADERS:
            (assets / header).write_text('// synthetic header\n')
        target = project / '.local' / 'sketch' / 'flappy'
        target.mkdir(parents=True)
        (target / 'preserve.txt').write_text('existing sketch')
        (project / '.local' / 'display-profile.json').write_text('"invalid"')
        with mock.patch.object(build, 'ROOT', project), self.assertRaises(SystemExit):
            build.prepare_sketch(assets)
        self.assertEqual((target / 'preserve.txt').read_text(), 'existing sketch')

    def test_build_cli_forwards_explicit_display_and_rejects_invalid_choice(self):
        sketch = self.directory / 'sketch'
        with mock.patch.object(build, 'prepare_sketch', return_value=sketch) as stage, \
                mock.patch.object(sys, 'argv', ['build.py', '--check-only', '--display', 'stickc-plus']), \
                contextlib.redirect_stdout(io.StringIO()):
            build.main()
        self.assertEqual(stage.call_args.args[3], 'stickc-plus')
        self.assertIsNone(stage.call_args.args[4])
        with mock.patch.object(build, 'prepare_sketch') as stage, \
                mock.patch.object(sys, 'argv', ['build.py', '--display', 'unknown']), \
                contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                build.main()
        self.assertEqual(raised.exception.code, 2)
        stage.assert_not_called()

    def test_build_cli_includes_core_library_and_check_only_avoids_setup(self):
        import setup_nes
        sketch = self.directory / 'sketch'
        library = self.directory / 'PeakNES'
        image = self.directory / '.local/build/flappy.ino.bin'
        image.parent.mkdir(parents=True)
        image.write_bytes(b'synthetic firmware')
        with mock.patch.object(build, 'prepare_sketch', return_value=sketch) as stage, \
                mock.patch.object(build, 'ROOT', self.directory), \
                mock.patch.object(build, 'arduino_cli', return_value='arduino-cli'), \
                mock.patch.object(build, 'ensure_config', return_value=self.directory / 'config'), \
                mock.patch.object(setup_nes, 'prepare', return_value=library) as prepare, \
                mock.patch.object(build.subprocess, 'run') as execute, \
                mock.patch.object(sys, 'argv', ['build.py', '--assets-dir', str(self.directory)]), \
                contextlib.redirect_stdout(io.StringIO()):
            build.main()
        prepare.assert_called_once_with()
        stage.assert_called_once_with(self.directory.resolve(), None, None, None, None, None, None, None,
                                     public=False, library=False, board='stickc')
        command = execute.call_args.args[0]
        self.assertEqual(command[command.index('--library') + 1], str(library))
        self.assertEqual(command[-1], str(sketch))
        with mock.patch.object(build, 'prepare_sketch', return_value=sketch) as stage, \
                mock.patch.object(setup_nes, 'prepare') as prepare, \
                mock.patch.object(sys, 'argv', ['build.py', '--check-only', '--rom-dir', str(self.directory),
                                              '--tetris-dir', str(self.directory / 'tetris')]), \
                contextlib.redirect_stdout(io.StringIO()):
            build.main()
        self.assertEqual(stage.call_args.args[2], self.directory.resolve())
        self.assertEqual(stage.call_args.args[4], (self.directory / 'tetris').resolve())
        prepare.assert_not_called()

    def test_geometry_default_explicit_and_legacy_positional_staging_preserve_other_games(self):
        project, assets = self.make_build_project()
        mario = self.directory / 'mario'
        tetris = self.directory / 'tetris'
        zero = self.directory / 'zero'
        default_geometry = project / '.local' / 'geometry-rom'
        explicit_geometry = self.directory / 'geometry'
        imports = ((mario, fixture(prg=2), 'Local Mario'),
                   (tetris, fixture(1, 2, 0), 'Local Tetris'),
                   (zero, fixture(118, 4, 2), 'Local Zero'),
                   (default_geometry, fixture(4, 2, 1), 'Default Dash'),
                   (explicit_geometry, fixture(4, 4, 1), 'Explicit Dash'))
        for folder, rom, title in imports:
            importer.generate(rom, folder, title, rights_acknowledged=True)
        originals = {folder: (folder / 'generated_rom.h').read_bytes() for folder, _, _ in imports}
        other_games = (('generated_rom.h', mario, 'FlappyRom'),
                       ('generated_tetris_rom.h', tetris, 'TetrisRom'),
                       ('generated_zero_rom.h', zero, 'TetrisZeroRom'))
        with mock.patch.object(build, 'ROOT', project):
            for mode in ('legacy', 'positional', 'keyword'):
                with self.subTest(mode=mode):
                    if mode == 'legacy':
                        target = build.prepare_sketch(assets, None, mario, 'stickc-plus', tetris, zero)
                    elif mode == 'positional':
                        target = build.prepare_sketch(assets, None, mario, 'stickc-plus', tetris, zero, explicit_geometry)
                    else:
                        target = build.prepare_sketch(assets, None, mario, 'stickc-plus', tetris, zero,
                                                      geometry=explicit_geometry)
                    for filename, folder, namespace in other_games:
                        expected = originals[folder].decode().replace('namespace FlappyRom', 'namespace ' + namespace)
                        self.assertEqual((target / filename).read_text(), expected)
                    chosen = default_geometry if mode == 'legacy' else explicit_geometry
                    expected = originals[chosen].decode().replace('namespace FlappyRom', 'namespace GeometryRom')
                    geometry_header = (target / 'generated_geometry_rom.h').read_text()
                    self.assertEqual(geometry_header, expected)
                    self.assertNotIn('namespace FlappyRom', geometry_header)
                    self.assertIn('#define M5FLAP_DISPLAY_PROFILE 2', (target / 'build_display.h').read_text())
        for folder, rom, _ in imports:
            self.assertEqual((folder / 'generated_rom.h').read_bytes(), originals[folder])
            self.assertEqual((folder / 'source' / 'rom.nes').read_bytes(), rom)

    def test_geometry_absent_default_and_missing_explicit_header_preserve_staged_sketch(self):
        project, assets = self.make_build_project()
        incomplete = project / '.local' / 'geometry-rom'
        incomplete.mkdir(parents=True)
        (incomplete / 'source.nes').write_bytes(fixture())
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets)
            geometry_header = (target / 'generated_geometry_rom.h').read_text()
            self.assertIn('namespace GeometryRom', geometry_header)
            self.assertIn('data = nullptr', geometry_header)
            self.assertIn('length = 0', geometry_header)
            self.assertIn('title[] = "Geometry Dash"', geometry_header)
            before = {path.name: path.read_bytes() for path in target.iterdir() if path.is_file()}
            for missing in (incomplete, self.directory / 'missing-geometry'):
                with self.subTest(missing=missing), self.assertRaisesRegex(SystemExit, 'Geometry Dash.*generated_rom.h'):
                    build.prepare_sketch(assets, geometry=missing)
                self.assertEqual(before, {path.name: path.read_bytes() for path in target.iterdir() if path.is_file()})

    def test_build_cli_forwards_geometry_without_reordering_existing_rom_arguments(self):
        sketch = self.directory / 'sketch'
        mario, tetris, zero, geometry = (self.directory / name for name in ('mario', 'tetris', 'zero', 'geometry'))
        with mock.patch.object(build, 'prepare_sketch', return_value=sketch) as stage, \
                mock.patch.object(sys, 'argv', ['build.py', '--check-only', '--rom-dir', str(mario),
                                              '--tetris-dir', str(tetris), '--zero-dir', str(zero),
                                              '--geometry-dir', str(geometry)]), \
                contextlib.redirect_stdout(io.StringIO()):
            build.main()
        self.assertEqual(stage.call_args.args[2], mario.resolve())
        self.assertEqual(stage.call_args.args[4], tetris.resolve())
        self.assertEqual(stage.call_args.args[5], zero.resolve())
        self.assertEqual(stage.call_args.args[6], geometry.resolve())

    def test_zero_stages_separately_and_missing_override_preserves_sketch(self):
        project, assets = self.make_build_project()
        zero = project / '.local' / 'tetris-zero-rom'
        rom = altered(fixture(118, 4, 2), 8, 1)
        receipt = importer.generate(rom, zero, 'Local Zero', rights_acknowledged=True)
        self.assertEqual(receipt['mapper'], 118)
        self.assertEqual((zero / 'source' / 'rom.nes').read_bytes(), rom)
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets)
            self.assertIn('namespace TetrisZeroRom', (target / 'generated_zero_rom.h').read_text())
            self.assertIn('Local Zero', (target / 'generated_zero_rom.h').read_text())
            self.assertIn('length = 0', (target / 'generated_rom.h').read_text())
            self.assertIn('length = 0', (target / 'generated_tetris_rom.h').read_text())
            before = {p.name: p.read_bytes() for p in target.iterdir() if p.is_file()}
            with self.assertRaisesRegex(SystemExit, 'Tetris Zero'):
                build.prepare_sketch(assets, zero=self.directory / 'missing')
            self.assertEqual(before, {p.name: p.read_bytes() for p in target.iterdir() if p.is_file()})

    def test_mapper_118_banks_and_four_screen_validation(self):
        for prg in (2, 4, 8, 16, 32):
            for chr_banks in (1, 2, 4, 8, 16, 32):
                metadata = importer.inspect_rom(fixture(118, prg, chr_banks))
                self.assertEqual(metadata['mapper'], 118)
        for prg, chr_banks in ((1, 2), (3, 2), (4, 0), (4, 3)):
            with self.assertRaisesRegex(importer.RomError, 'Mapper 118'):
                importer.inspect_rom(fixture(118, prg, chr_banks))
        with self.assertRaisesRegex(importer.RomError, 'two nametable'):
            importer.inspect_rom(altered(fixture(118, 4, 2), 6, 0x68))

    def test_mapper_four_banks_four_screen_and_multiregion(self):
        for nes2 in (False, True):
            for prg in (2, 4, 8, 16, 32):
                for chr_banks in (0, 1, 2, 4, 8, 16, 32):
                    with self.subTest(nes2=nes2, prg=prg, chr=chr_banks):
                        metadata = importer.inspect_rom(fixture(4, prg, chr_banks, nes2=nes2))
                        self.assertEqual(metadata['mapper'], 4)
                        self.assertEqual(metadata['prg_bytes'], prg * 16384)
                        self.assertEqual(metadata['chr_bytes'], chr_banks * 8192)
            for prg, chr_banks in ((0, 2), (1, 2), (3, 2), (33, 2), (4, 3), (4, 33)):
                with self.subTest(nes2=nes2, prg=prg, chr=chr_banks):
                    with self.assertRaisesRegex(importer.RomError, 'Mapper 4'):
                        importer.inspect_rom(fixture(4, prg, chr_banks, nes2=nes2))
        rom = fixture(4, 32, 32, nes2=True)
        rom = altered(altered(altered(rom, 6, 0x4a), 10, 0x70), 12, 2)
        receipt = importer.generate(rom, self.output, 'Synthetic MMC3', rights_acknowledged=True)
        self.assertEqual(receipt['bytes'], 786448)
        self.assertTrue(receipt['four_screen'])
        self.assertTrue(receipt['battery'])
        self.assertEqual(receipt['prg_ram_header'], 0x70)
        self.assertEqual(receipt['timing'], 'Multi-region')
        self.assertEqual(receipt['frame_rate'], 60)
        self.assertEqual((self.output / 'source' / 'rom.nes').read_bytes(), rom)
        for data in (rom[:-1], rom + b'x'):
            with self.assertRaisesRegex(importer.RomError, 'match its header exactly'):
                importer.inspect_rom(data)

    def test_mapper_four_explicit_ram_is_exactly_one_8k_bank(self):
        for battery in (False, True):
            rom = fixture(4, 2, 1, nes2=True)
            rom = altered(rom, 6, rom[6] | (2 if battery else 0))
            for value in range(256):
                data = altered(rom, 10, value)
                if value in (0, 7) or (battery and value == 0x70):
                    self.assertEqual(importer.inspect_rom(data)['prg_ram_header'], value)
                else:
                    with self.subTest(battery=battery, ram=value):
                        with self.assertRaisesRegex(importer.RomError, 'PRG RAM and NVRAM'):
                            importer.inspect_rom(data)
        for mapper, prg, chr_banks in ((0, 2, 1), (1, 2, 1), (37, 16, 32), (118, 2, 1)):
            rom = fixture(mapper, prg, chr_banks, nes2=True)
            rom = altered(rom, 6, rom[6] | 2)
            for value in (7, 0x70):
                with self.subTest(mapper=mapper, ram=value):
                    with self.assertRaisesRegex(importer.RomError, 'PRG RAM and NVRAM'):
                        importer.inspect_rom(altered(rom, 10, value))


if __name__ == '__main__':
    unittest.main()
