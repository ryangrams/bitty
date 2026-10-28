"""Check private Jetpack imports with synthetic package data only."""
import importlib.util
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
import warnings
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('import_jetpack', ROOT / 'tools/import_jetpack.py')
importer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(importer)


def archive_bytes(entries):
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            archive.writestr(name, data)
    return output.getvalue()


def binary_xml():
    strings = b'settings\x00setting\x00name\x00value\x00GRAVITY\x000.15\x00'
    offset = {value.decode(): strings.index(value + b'\x00') for value in strings.split(b'\x00') if value}
    child = b'\x01' + struct.pack('<II', offset['setting'], 2)
    child += struct.pack('<IIII', offset['name'], offset['GRAVITY'], offset['value'], offset['0.15'])
    child += struct.pack('<I', 0)
    root = b'\x01' + struct.pack('<III', offset['settings'], 0, 1) + child
    return b'bxml\x01\x00\x03\x00' + struct.pack('<I', len(strings)) + strings + struct.pack('<I', 1) + root


def texture(width=128, height=64):
    header = bytearray(52)
    header[:4] = b'TEX\x01'
    header[7:15] = bytes((1, 4, 2, 4, 3, 4, 4, 4))
    struct.pack_into('<II', header, 28, width, height)
    pixels = [0] * (width * height)
    for row in range(2):
        for column in range(4):
            for y in range(10, 20):
                for x in range(10, 20):
                    pixels[(row * 32 + y) * width + column * 32 + x] = 0xf00f
    return bytes(header) + struct.pack('<' + str(len(pixels)) + 'H', *pixels)


class JetpackImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='jetpack-import-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def entries(self):
        config = {'coin_pattern_file': {'coin_pattern': [
            {'filename': 'patterns/coinPattern1.txt', 'probability': {'value': 5, 'startDistance': 0}},
            {'filename': 'patterns/event.txt', 'probability': {'value': 2, 'leveltag': 'event'}},
        ]}}
        return [(importer.BASE_CONFIG, json.dumps(config)), ('patterns/coinPattern1.txt', '010\n101'),
                ('patterns/cashDash1.txt', '1 \n\n 1'), ('patterns/event.txt', '11'),
                ('xml/settings.xml', binary_xml())]

    def package(self, entries=None, nested=False):
        data = archive_bytes(self.entries() if entries is None else entries)
        if nested:
            data = archive_bytes([('split_InstallPack.apk', archive_bytes([('assets/assets.zip', data)]))])
        source = self.directory / ('fixture.apkm' if nested else 'fixture.apk')
        source.write_bytes(data)
        return source

    def run_import(self, source=None, **kwargs):
        return importer.import_package(source or self.package(), self.directory / 'private', accept_rights=True, **kwargs)

    def test_nested_import_is_deterministic_and_column_sorted(self):
        source = self.package(nested=True)
        first = self.run_import(source)
        before = {path.name: path.read_bytes() for path in (self.directory / 'private').iterdir()}
        second = self.run_import(source)
        self.assertEqual(first, second)
        self.assertEqual(before, {path.name: path.read_bytes() for path in (self.directory / 'private').iterdir()})
        self.assertEqual(first['point_count'], 7)
        self.assertEqual(first['base_pattern_count'], 1)
        header = before['jetpack_imported.h'].decode()
        self.assertIn('HasRunner = false', header)
        self.assertIn('0x0100,0x0001,0x0102', header)
        self.assertEqual(importer.pattern_points(b'010\n101')[2], [256, 1, 258])
        config = json.loads(before['reference-config.json'])
        self.assertEqual(config['xml/settings.xml'][0]['children'][0]['attrs']['value'], '0.15')

    def test_direct_apk_assets_prefix_is_supported(self):
        entries = [('assets/' + name, data) for name, data in self.entries()]
        self.assertEqual(self.run_import(self.package(entries))['point_count'], 7)

    def test_rights_are_required_before_output(self):
        with self.assertRaisesRegex(importer.ImportError, 'accept-rights'):
            importer.import_package(self.package(), self.directory / 'private')
        self.assertFalse((self.directory / 'private').exists())

    def test_public_output_is_rejected(self):
        with self.assertRaisesRegex(importer.ImportError, 'under .local'):
            importer.import_package(self.package(), ROOT / 'generated', accept_rights=True)

    def test_unsafe_names_and_duplicate_entries_are_rejected(self):
        for name in ('../escape', '/escape', 'C:/escape', 'bad\\escape', 'a/../escape'):
            with self.subTest(name=name), self.assertRaisesRegex(importer.ImportError, 'Unsafe'):
                self.run_import(self.package(self.entries() + [(name, 'data')]))
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            source = self.package(self.entries() + [('patterns/coinPattern1.txt', '1')])
        with self.assertRaisesRegex(importer.ImportError, 'Duplicate'):
            self.run_import(source)

    def test_malformed_patterns_do_not_replace_existing_outputs(self):
        self.run_import()
        output = self.directory / 'private' / 'jetpack_imported.h'
        before = output.read_bytes()
        for pattern in ('', '1' * 257, '\n'.join(['1'] * 41), '2', '\t1', '\u00e9'):
            entries = [(name, pattern if name == 'patterns/coinPattern1.txt' else data) for name, data in self.entries()]
            with self.subTest(pattern=pattern[:10]), self.assertRaises(importer.ImportError):
                self.run_import(self.package(entries))
            self.assertEqual(output.read_bytes(), before)

    def test_size_entry_and_compression_limits(self):
        with mock.patch.object(importer, 'MAX_PACKAGE', 8), self.assertRaisesRegex(importer.ImportError, '512 MiB'):
            self.run_import()
        with mock.patch.object(importer, 'MAX_ENTRIES', 2), self.assertRaisesRegex(importer.ImportError, 'too many'):
            self.run_import()
        entries = self.entries() + [('patterns/bomb.txt', '0' * 65537)]
        with self.assertRaisesRegex(importer.ImportError, 'size or compression'):
            self.run_import(self.package(entries))
        with self.assertRaisesRegex(importer.ImportError, '512 occupied'):
            importer.pattern_points((b'1' * 200 + b'\n') * 3)

    def test_bad_bxml_and_json_are_rejected(self):
        for data in (b'bxml', binary_xml()[:-1], binary_xml() + b'garbage'):
            entries = [(name, data if name == 'xml/settings.xml' else value) for name, value in self.entries()]
            with self.subTest(data=data[:4]), self.assertRaises(importer.ImportError):
                self.run_import(self.package(entries))
        entries = [(name, '{broken' if name == importer.BASE_CONFIG else value) for name, value in self.entries()]
        with self.assertRaisesRegex(importer.ImportError, 'JSON'):
            self.run_import(self.package(entries))

    def test_missing_game_data_and_invalid_zip(self):
        with self.assertRaisesRegex(importer.ImportError, 'No supported'):
            self.run_import(self.package([('readme.txt', 'synthetic')]))
        source = self.directory / 'bad.apk'
        source.write_bytes(b'not a zip')
        with self.assertRaisesRegex(importer.ImportError, 'Invalid or unreadable'):
            self.run_import(source)
        entries = [(name, data) for name, data in self.entries() if name != 'patterns/coinPattern1.txt']
        with self.assertRaisesRegex(importer.ImportError, 'No nonempty base'):
            self.run_import(self.package(entries))

    def test_unresolved_base_references_are_reported_without_guessing_aliases(self):
        entries = self.entries()
        config = json.loads(entries[0][1])
        config['coin_pattern_file']['coin_pattern'].append({
            'filename': 'patterns/missing.txt', 'probability': {'value': 5}})
        entries[0] = (importer.BASE_CONFIG, json.dumps(config))
        receipt = self.run_import(self.package(entries))
        self.assertEqual(receipt['missing_base_patterns'], ['patterns/missing.txt'])
        self.assertEqual(receipt['base_pattern_count'], 1)

    def test_optional_runner_uses_fixed_frames_and_no_external_library(self):
        entries = self.entries() + [(name, texture()) for name in importer.RUNNER_SOURCES]
        receipt = self.run_import(self.package(entries), original_runner=True)
        header = (self.directory / 'private' / 'jetpack_imported.h').read_text()
        self.assertIn('HasRunner = true', header)
        self.assertIn('FrameCount = 6', header)
        self.assertIn('0xf800', header)
        self.assertIn('approximations', receipt['runner']['method'])

    def test_malformed_runner_dimensions_are_rejected(self):
        bad = bytearray(texture())
        struct.pack_into('<I', bad, 28, 0xffffffff)
        entries = self.entries() + [(name, bytes(bad)) for name in importer.RUNNER_SOURCES]
        with self.assertRaisesRegex(importer.ImportError, 'dimensions'):
            self.run_import(self.package(entries), original_runner=True)

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler unavailable')
    def test_generated_header_compiles_with_and_without_runner(self):
        entries = self.entries() + [(name, texture()) for name in importer.RUNNER_SOURCES]
        source = self.package(entries)
        probe = self.directory / 'probe.cpp'
        for enabled in (False, True):
            self.run_import(source, original_runner=enabled)
            probe.write_text('#include "jetpack_imported.h"\n'
                             'using namespace flappy::JetpackImported;\n'
                             'static_assert(PatternCount == 3, "pattern count");\n'
                             'static_assert(MaxPatternPoints <= 512, "mask capacity");\n'
                             'static_assert(sizeof(RunnerPixels[0]) == 600, "runner dimensions");\n'
                             'static_assert(FrameCount == %d, "frame count");\n' % (6 if enabled else 0))
            subprocess.run([shutil.which('c++'), '-std=c++11', '-Wall', '-Wextra', '-Werror',
                            '-fsyntax-only', '-I' + str(self.directory / 'private'), str(probe)], check=True)


if __name__ == '__main__':
    unittest.main()
