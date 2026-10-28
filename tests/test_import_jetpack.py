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


def binary_xml_nodes(roots):
    strings = bytearray()
    offsets = {}

    def text(value):
        if value not in offsets:
            offsets[value] = len(strings)
            strings.extend(value.encode() + b'\x00')
        return struct.pack('<I', offsets[value])

    def node(value):
        attrs = value.get('attrs', {})
        children = value.get('children', [])
        result = b'\x01' + text(value['tag']) + struct.pack('<I', len(attrs))
        result += b''.join(text(key) + text(item) for key, item in attrs.items())
        return result + struct.pack('<I', len(children)) + b''.join(node(child) for child in children)

    data = b''.join(node(root) for root in roots)
    return b'bxml\x01\x00\x03\x00' + struct.pack('<I', len(strings)) + strings + struct.pack('<I', len(roots)) + data


def rocket_fixture():
    def actor(delay, low, high, angle, speed, random='false'):
        return {'tag': 'missile', 'attrs': {'scale': '1', 'delay': str(delay)}, 'children': [
            {'tag': 'position', 'attrs': {'random': random, 'range': 'true', 'lower_y': str(low), 'upper_y': str(high)}},
            {'tag': 'flight', 'attrs': {'dud': 'false', 'ricochet': 'true', 'ricochet_angle': str(angle), 'speed': str(speed)}},
        ]}
    return [{'tag': 'missile_file', 'children': [
        {'tag': 'missileSpeed', 'attrs': {'value': '3'}},
        {'tag': 'chaseSpeed', 'attrs': {'value': '0.5'}},
        {'tag': 'missile_pattern', 'children': [
            {'tag': 'probability', 'attrs': {'value': '7', 'startDistance': '0'}},
            actor(-100, -60, 60, 12, 2), actor(-80, 30, -30, -12, 1, 'true'),
        ]},
    ]}]


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

    def test_profiles_preserve_distance_weights_and_empty_choices(self):
        sam = {'coin_pattern_file': {'coin_pattern': [
            {'filename': 'patterns/blank.txt', 'probability': {'value': 40, 'startDistance': 20}},
            {'filename': 'patterns/coinPattern1.txt', 'probability': {'value': 1, 'startDistance': 20}},
            {'filename': 'patterns/missingSAM.txt', 'probability': {'value': 2}},
        ]}}
        gravity = {'coin_pattern_file': {'coin_pattern': [
            {'filename': 'patterns/coinPattern1.txt', 'probability': [
                {'value': 0, 'startDistance': 0}, {'value': 5, 'startDistance': 20},
                {'value': 12, 'startDistance': 0, 'leveltag': 'event'}]},
        ]}}
        configs = dict((label, name) for _, label, name in importer.PROFILE_CONFIGS)
        entries = self.entries() + [('patterns/blank.txt', '00\n00'),
                                    (configs['SAM'], json.dumps(sam)),
                                    (configs['Gravity'], json.dumps(gravity))]
        receipt = self.run_import(self.package(entries))
        header = (self.directory / 'private' / 'jetpack_imported.h').read_text()
        self.assertEqual(receipt['profile_ids'], {'Default': 0, 'SAM': 2, 'Gravity': 4})
        self.assertEqual(receipt['profile_weight_count'], 5)
        self.assertEqual(receipt['missing_profile_patterns']['SAM'], ['patterns/missingSAM.txt'])
        patterns = {pattern['name']: pattern for pattern in receipt['patterns']}
        self.assertEqual(patterns['patterns/blank.txt']['count'], 0)
        self.assertEqual(patterns['patterns/blank.txt']['flags'], 16)
        self.assertEqual(patterns['patterns/coinPattern1.txt']['flags'], 81)
        self.assertIn('#define M5FLAP_JETPACK_PROFILE_WEIGHTS 1', header)
        self.assertIn('{0,20,4000,2}', header)
        self.assertIn('{2,20,100,2}', header)
        self.assertIn('{2,0,0,4}', header)
        self.assertIn('{2,20,500,4}', header)

    def test_malformed_profile_weights_preserve_existing_import(self):
        self.run_import()
        output = self.directory / 'private' / 'jetpack_imported.h'
        before = output.read_bytes()
        name = dict((label, path) for _, label, path in importer.PROFILE_CONFIGS)['SAM']
        for probability in ({'value': -1}, {'value': 656}, {'value': 'NaN'},
                            {'value': 1, 'startDistance': -1}, {'value': 1, 'startDistance': 65536},
                            {'value': 1, 'startDistance': 0.5}, [],
                            [{'value': 1}, {'value': 2}]):
            config = {'coin_pattern_file': {'coin_pattern': [
                {'filename': 'patterns/coinPattern1.txt', 'probability': probability}]}}
            with self.subTest(probability=probability), self.assertRaises(importer.ImportError):
                self.run_import(self.package(self.entries() + [(name, json.dumps(config))]))
            self.assertEqual(output.read_bytes(), before)

    def test_optional_runner_uses_fixed_frames_and_no_external_library(self):
        entries = self.entries() + [(name, texture()) for name in importer.RUNNER_SOURCES]
        receipt = self.run_import(self.package(entries), original_runner=True)
        header = (self.directory / 'private' / 'jetpack_imported.h').read_text()
        self.assertIn('HasRunner = true', header)
        self.assertIn('FrameCount = 6', header)
        self.assertIn('0xf800', header)
        self.assertIn('approximations', receipt['runner']['method'])

    def test_optional_rocket_waves_preserve_timing_lanes_and_flags(self):
        roots = rocket_fixture()
        data = binary_xml_nodes(roots)
        waves = importer.rocket_waves(roots)
        self.assertEqual(importer.rocket_waves(importer.BinaryXML(data).decode()), waves)
        self.assertEqual(waves['patterns'], [{'offset': 0, 'count': 2, 'weight': 700}])
        self.assertEqual(waves['missiles'][0], {'launchTicks': 50, 'warningTicks': 20,
                         'laneMinQ8': -128, 'laneMaxQ8': 128, 'angleQ8': 3072,
                         'speedQ8': 512, 'direction': -1, 'flags': 6})
        self.assertEqual(waves['missiles'][1]['laneMinQ8'], -64)
        self.assertEqual(waves['missiles'][1]['laneMaxQ8'], 64)
        self.assertEqual(waves['missiles'][1]['flags'], 7)
        receipt = self.run_import(self.package(self.entries() + [(importer.ROCKET_CONFIG, data)]))
        header = (self.directory / 'private' / 'jetpack_imported.h').read_text()
        self.assertEqual((receipt['rocket_pattern_count'], receipt['rocket_missile_count']), (1, 2))
        self.assertIn('#define M5FLAP_JETPACK_ROCKET_WAVES 1', header)
        self.assertIn('RocketMissileSpeedQ8 = 768', header)
        self.assertIn('RocketChaseSpeedQ8 = 128', header)
        self.assertIn('{50,20,-128,128,3072,512,-1,6}', header)
        self.run_import(self.package())
        self.assertNotIn('M5FLAP_JETPACK_ROCKET_WAVES', (self.directory / 'private' / 'jetpack_imported.h').read_text())

    def test_malformed_rocket_config_preserves_existing_import(self):
        self.run_import()
        output = self.directory / 'private' / 'jetpack_imported.h'
        before = output.read_bytes()
        for field, value in (('delay', '-121'), ('delay', '1.5'), ('scale', '2')):
            roots = rocket_fixture()
            roots[0]['children'][2]['children'][1]['attrs'][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(importer.ImportError):
                self.run_import(self.package(self.entries() + [(importer.ROCKET_CONFIG, binary_xml_nodes(roots))]))
            self.assertEqual(output.read_bytes(), before)
        for field, value in (('upper_y', 'NaN'), ('upper_y', '241'), ('random', 'yes')):
            roots = rocket_fixture()
            roots[0]['children'][2]['children'][1]['children'][0]['attrs'][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(importer.ImportError):
                importer.rocket_waves(roots)
        roots = rocket_fixture()
        roots[0]['children'][2]['children'] += roots[0]['children'][2]['children'][1:] * 4
        with self.assertRaisesRegex(importer.ImportError, 'missile count'):
            importer.rocket_waves(roots)
        roots = rocket_fixture()
        roots[0]['children'] += roots[0]['children'][2:] * importer.MAX_ROCKET_PATTERNS
        with self.assertRaisesRegex(importer.ImportError, 'pattern count'):
            importer.rocket_waves(roots)

    def test_malformed_runner_dimensions_are_rejected(self):
        bad = bytearray(texture())
        struct.pack_into('<I', bad, 28, 0xffffffff)
        entries = self.entries() + [(name, bytes(bad)) for name in importer.RUNNER_SOURCES]
        with self.assertRaisesRegex(importer.ImportError, 'dimensions'):
            self.run_import(self.package(entries), original_runner=True)

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler unavailable')
    def test_generated_header_compiles_with_and_without_runner(self):
        entries = self.entries() + [(name, texture()) for name in importer.RUNNER_SOURCES]
        entries.append((importer.ROCKET_CONFIG, binary_xml_nodes(rocket_fixture())))
        source = self.package(entries)
        probe = self.directory / 'probe.cpp'
        for enabled in (False, True):
            self.run_import(source, original_runner=enabled)
            probe.write_text('#include "jetpack_imported.h"\n'
                             'using namespace flappy::JetpackImported;\n'
                             'static_assert(PatternCount == 3, "pattern count");\n'
                             'static_assert(MaxPatternPoints <= 512, "mask capacity");\n'
                             'static_assert(ProfileWeightCount == 1, "profile weights");\n'
                             'static_assert(ProfileSAM == 2, "profile ID");\n'
                             'static_assert(RocketPatternCount == 1, "Rocket patterns");\n'
                             'static_assert(RocketMissileCount == 2, "Rocket missiles");\n'
                             'static_assert(sizeof(RunnerPixels[0]) == 600, "runner dimensions");\n'
                             'static_assert(FrameCount == %d, "frame count");\n' % (6 if enabled else 0))
            subprocess.run([shutil.which('c++'), '-std=c++11', '-Wall', '-Wextra', '-Werror',
                            '-fsyntax-only', '-I' + str(self.directory / 'private'), str(probe)], check=True)


if __name__ == '__main__':
    unittest.main()


class PlacementConfigTests(unittest.TestCase):
    def test_vertical_weights_and_synthetic_obstacle_records(self):
        placement_weights, obstacle_records, header, ImportError = importer.placement_weights, importer.obstacle_records, importer.header, importer.ImportError
        self.assertEqual(placement_weights({'TopWeight': {'value': '25'}}), [25,0,0,0])
        for bad in (-1, 101, 'NaN', '2.5', 'abc'):
            with self.assertRaises(ImportError):
                placement_weights({'TopWeight': {'value': bad}})
        children = [{'tag': k, 'attrs': {'value': v}} for k,v in
                    [('minScale','48'), ('maxScale','84'), ('startAngle','45'), ('rotateTime','120')]]
        children.append({'tag':'probability','attrs':{'value':'7','startDistance':'20'}})
        records = obstacle_records([{'tag':'obstacle_pattern_file','children':[
            {'tag':'BottomWeight','attrs':{'value':'75'}},
            {'tag':'obstacle_pattern','children':children}]}], 3)
        self.assertEqual(records[0]['minimum'], 48)
        self.assertEqual(records[0]['maximum'], 84)
        self.assertEqual(records[0]['placement'], [0,75,0,0])
        self.assertEqual(records[0]['weight'], 700)
        self.assertEqual(records[0]['start'], 20)
        source = header([dict(offset=0,count=1,width=1,height=1,weight=1,flags=1)], [0], [],
                        placements=[[0,0,75,0,0]], obstacles=records)
        self.assertIn('M5FLAP_JETPACK_OBSTACLES', source)
        self.assertIn('{48,84,45,120,20,700,3,3,0,0,75,0,0}', source)
