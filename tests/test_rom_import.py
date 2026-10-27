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
            (fixture(1), 'mapper 1'), (fixture(0, 3), 'Mapper 0'),
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

    def test_build_stages_empty_explicit_or_default_rom_without_dependency_downloads(self):
        assets = self.directory / 'assets'
        assets.mkdir()
        for header in build.HEADERS:
            (assets / header).write_text('// synthetic header\n')
        project = self.directory / 'project'
        firmware = project / 'firmware' / 'flappy'
        firmware.mkdir(parents=True)
        (firmware / 'flappy.ino').write_text('// synthetic sketch\n')
        with mock.patch.object(build, 'ROOT', project):
            target = build.prepare_sketch(assets)
            self.assertIn('data = nullptr', (target / 'generated_rom.h').read_text())
            self.assertTrue((target / 'generated_skins.h').is_file())
            explicit = self.directory / 'explicit'
            importer.generate(fixture(), explicit, 'Fixture', rights_acknowledged=True)
            target = build.prepare_sketch(assets, None, explicit)
            self.assertEqual((target / 'generated_rom.h').read_bytes(), (explicit / 'generated_rom.h').read_bytes())
            default = project / '.local' / 'nes-rom'
            importer.generate(fixture(), default, 'Default', rights_acknowledged=True)
            target = build.prepare_sketch(assets)
            self.assertEqual((target / 'generated_rom.h').read_bytes(), (default / 'generated_rom.h').read_bytes())
            (target / 'preserve.txt').write_text('old sketch')
            with self.assertRaisesRegex(SystemExit, 'generated_rom.h'):
                build.prepare_sketch(assets, None, self.directory / 'missing')
            self.assertTrue((target / 'preserve.txt').exists())


    def test_build_cli_includes_core_library_and_check_only_avoids_setup(self):
        import setup_nes
        sketch = self.directory / 'sketch'
        library = self.directory / 'PeakNES'
        with mock.patch.object(build, 'prepare_sketch', return_value=sketch) as stage, \
                mock.patch.object(build, 'arduino_cli', return_value='arduino-cli'), \
                mock.patch.object(build, 'ensure_config', return_value=self.directory / 'config'), \
                mock.patch.object(setup_nes, 'prepare', return_value=library) as prepare, \
                mock.patch.object(build.subprocess, 'run') as execute, \
                mock.patch.object(sys, 'argv', ['build.py', '--assets-dir', str(self.directory)]), \
                contextlib.redirect_stdout(io.StringIO()):
            build.main()
        prepare.assert_called_once_with()
        stage.assert_called_once_with(self.directory.resolve(), None, None)
        command = execute.call_args.args[0]
        self.assertEqual(command[command.index('--library') + 1], str(library))
        self.assertEqual(command[-1], str(sketch))
        with mock.patch.object(build, 'prepare_sketch', return_value=sketch), \
                mock.patch.object(setup_nes, 'prepare') as prepare, \
                mock.patch.object(sys, 'argv', ['build.py', '--check-only', '--rom-dir', str(self.directory)]), \
                contextlib.redirect_stdout(io.StringIO()):
            build.main()
        prepare.assert_not_called()


if __name__ == '__main__':
    unittest.main()
