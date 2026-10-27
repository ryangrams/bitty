"""Check the private Tetris preparation workflow with synthetic cartridge images."""
from __future__ import annotations

import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import import_rom
import prepare_tetris as kit
from test_bps import action, patch as bps_patch
from test_ips import literal, patch as ips_patch
from test_rom_import import fixture


class TetrisPreparationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='m5-tetris-prepare-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.output = self.root / 'private'
        self.source = fixture(1, 2, 2)
        self.gym = self.source[:100] + b'GYM fixture' + self.source[111:]
        self.zero = fixture(118, 4, 2)
        self.rom = self.root / 'base.nes'
        self.gym_patch = self.root / 'gym.bps'
        self.zero_patch = self.root / 'zero.ips'
        self.rom.write_bytes(self.source)
        self.gym_bytes = bps_patch(self.source, self.gym, action(1, len(self.gym), self.gym))
        self.zero_bytes = ips_patch(*(literal(offset, self.zero[offset:offset + 60000])
                                      for offset in range(0, len(self.zero), 60000)))
        self.gym_patch.write_bytes(self.gym_bytes)
        self.zero_patch.write_bytes(self.zero_bytes)
        self.production = copy.deepcopy(kit.ENGINE_VERSIONS)
        versions = copy.deepcopy(self.production)
        # Synthetic fixtures exercise the workflow without embedding game content.
        versions['gym']['crc32'] = f'{zlib.crc32(self.gym):08x}'
        versions['zero']['crc32'] = f'{zlib.crc32(self.zero):08x}'
        self.version_patch = mock.patch.object(kit, 'ENGINE_VERSIONS', versions)
        self.version_patch.start()
        self.addCleanup(self.version_patch.stop)

    def prepare(self, **changes):
        arguments = {'gym_patch': self.gym_patch, 'zero_patch': self.zero_patch,
                     'output_root': self.output, 'rights_acknowledged': True}
        arguments.update(changes)
        return kit.prepare(self.rom, **arguments)

    def snapshot(self):
        return {path.relative_to(self.output).as_posix(): path.read_bytes()
                for path in self.output.rglob('*') if path.is_file()}

    def invoke(self, *extra):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return kit.main(['--rom', str(self.rom), '--gym-patch', str(self.gym_patch),
                             '--zero-patch', str(self.zero_patch), '--output-root', str(self.output), *extra])

    def test_production_versions_are_fixed_and_have_no_cli_bypass(self):
        self.assertEqual(self.production['gym']['crc32'], '37a5ec63')
        self.assertEqual(self.production['zero']['crc32'], '66462642')
        with mock.patch.object(kit, 'ENGINE_VERSIONS', self.production):
            with self.assertRaisesRegex(kit.PrepareError, 'expected 6.0.0 CRC32 37a5ec63'):
                self.prepare()
            with self.assertRaisesRegex(kit.PrepareError, 'expected MMC3_118_v2 CRC32 66462642'):
                self.prepare(gym_patch=None)
        self.assertFalse(self.output.exists())
        with self.assertRaises(SystemExit) as raised:
            self.invoke('--acknowledge-rights', '--allow-unsupported-testing')
        self.assertEqual(raised.exception.code, 2)
        self.assertFalse(self.output.exists())

    def test_rights_acknowledgment_and_patch_selection_precede_reads(self):
        with mock.patch.object(kit, '_read') as read:
            with self.assertRaisesRegex(kit.PrepareError, 'acknowledgment is required'):
                self.prepare(rights_acknowledged=False)
            with self.assertRaisesRegex(kit.PrepareError, 'Supply --gym-patch'):
                self.prepare(gym_patch=None, zero_patch=None)
            read.assert_not_called()
        with mock.patch.object(kit, 'prepare') as prepare:
            with self.assertRaises(SystemExit):
                self.invoke()
            prepare.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_both_engines_write_standard_imports_and_private_hash_receipts(self):
        receipt = self.prepare()
        self.assertTrue(receipt['rightsAcknowledged'])
        self.assertEqual([item['engine'] for item in receipt['engines']], ['gym', 'zero'])
        for name, target in (('gym', self.gym), ('zero', self.zero)):
            folder = self.output / kit.ENGINE_VERSIONS[name]['folder']
            self.assertEqual((folder / 'source/rom.nes').read_bytes(), target)
            self.assertIn('namespace FlappyRom', (folder / 'generated_rom.h').read_text())
            imported = json.loads((folder / 'import_receipt.json').read_text())
            prepared = json.loads((folder / 'preparation_receipt.json').read_text())
            patched = json.loads((folder / 'patch_receipt.json').read_text())
            self.assertTrue(imported['rightsAcknowledged'] and prepared['rightsAcknowledged'])
            self.assertEqual(imported['rom_sha256'], hashlib.sha256(target).hexdigest())
            self.assertEqual(prepared['target']['sha256'], imported['rom_sha256'])
            self.assertEqual(prepared['source']['sha256'], hashlib.sha256(self.source).hexdigest())
            self.assertEqual(prepared['expected_crc32'], f'{zlib.crc32(target):08x}')
            self.assertEqual(patched['target_sha256'], imported['rom_sha256'])
            self.assertEqual(prepared['source']['name'], 'base.nes')
            self.assertNotIn(str(self.root), (folder / 'preparation_receipt.json').read_text())
        self.assertEqual(self.rom.read_bytes(), self.source)
        self.assertEqual(self.gym_patch.read_bytes(), self.gym_bytes)
        self.assertEqual(self.zero_patch.read_bytes(), self.zero_bytes)
        self.assertFalse(list(self.output.glob('.tetris-prepare-*')))

    def test_default_private_folders_cli_and_one_patch_preserve_other_engine(self):
        with mock.patch.object(kit, 'ROOT', self.root):
            result = self.prepare(output_root=None)
        self.assertEqual(len(result['engines']), 2)
        self.assertTrue((self.root / '.local/tetris-rom/generated_rom.h').is_file())
        self.assertTrue((self.root / '.local/tetris-zero-rom/generated_rom.h').is_file())
        self.assertEqual(self.invoke('--acknowledge-rights'), 0)
        zero_folder = self.output / 'tetris-zero-rom'
        (zero_folder / 'unrelated-note.txt').write_text('keep this untouched')
        old_zero = {path.name: path.read_bytes() for path in zero_folder.iterdir() if path.is_file()}
        self.prepare(zero_patch=None)
        self.assertEqual(old_zero, {path.name: path.read_bytes() for path in zero_folder.iterdir() if path.is_file()})
        separate = self.root / 'zero-only'
        self.prepare(gym_patch=None, output_root=separate)
        self.assertTrue((separate / 'tetris-zero-rom/generated_rom.h').is_file())
        self.assertFalse((separate / 'tetris-rom').exists())

    def test_exact_bps_mirroring_normalization_is_recorded(self):
        normalized = self.source[:6] + bytes([self.source[6] ^ 1]) + self.source[7:]
        encoded = bps_patch(normalized, self.gym, action(1, len(self.gym), self.gym))
        self.gym_patch.write_bytes(encoded)
        self.prepare()
        receipt = json.loads((self.output / 'tetris-rom/patch_receipt.json').read_text())
        self.assertEqual(receipt['nes_header_normalization'],
                         {'header_byte': 6, 'original': self.source[6], 'patched_source': normalized[6]})
        self.assertEqual(self.rom.read_bytes(), self.source)

    def test_every_requested_result_is_validated_before_output_mutation(self):
        self.prepare()
        original = self.snapshot()
        self.zero_patch.write_bytes(b'bad IPS')
        with mock.patch.object(kit.tempfile, 'mkdtemp') as stage:
            with self.assertRaises(ValueError):
                self.prepare()
            stage.assert_not_called()
        self.assertEqual(self.snapshot(), original)
        self.assertEqual(self.rom.read_bytes(), self.source)
        self.output = self.root / 'new-output'
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_wrong_version_and_mapper_preserve_existing_imports(self):
        self.prepare()
        original = self.snapshot()
        changed = self.zero[:-1] + bytes([self.zero[-1] ^ 1])
        self.zero_patch.write_bytes(ips_patch(*(literal(offset, changed[offset:offset + 60000])
                                               for offset in range(0, len(changed), 60000))))
        with self.assertRaisesRegex(kit.PrepareError, 'result is unsupported'):
            self.prepare()
        self.assertEqual(self.snapshot(), original)
        wrong_mapper = fixture(0, 2, 1)
        self.gym_patch.write_bytes(bps_patch(self.source, wrong_mapper, action(1, len(wrong_mapper), wrong_mapper)))
        kit.ENGINE_VERSIONS['gym']['crc32'] = f'{zlib.crc32(wrong_mapper):08x}'
        with self.assertRaisesRegex(kit.PrepareError, 'result is unsupported'):
            self.prepare(zero_patch=None)
        self.assertEqual(self.snapshot(), original)

    def test_local_input_extensions_urls_missing_and_size_limits(self):
        for value in ('https://example.test/patch.bps', '-', 'wrong.ips', self.root / 'missing.bps'):
            with self.subTest(value=value), self.assertRaises((kit.PrepareError, OSError)):
                self.prepare(gym_patch=value)
        self.gym_patch.write_bytes(bytes(kit.MAX_BYTES + 1))
        with self.assertRaisesRegex(kit.PrepareError, '1 MiB'):
            self.prepare()
        self.assertFalse(self.output.exists())
        with self.assertRaises(kit.PrepareError):
            self.prepare(output_root='https://example.test/output')
        self.assertFalse(self.output.exists())

    def test_originals_inside_import_folders_are_rejected(self):
        inside = self.output / 'tetris-rom/source/rom.nes'
        inside.parent.mkdir(parents=True)
        inside.write_bytes(self.source)
        with self.assertRaisesRegex(kit.PrepareError, 'outside the generated import folders'):
            kit.prepare(inside, gym_patch=self.gym_patch, output_root=self.output, rights_acknowledged=True)
        self.assertEqual(inside.read_bytes(), self.source)
        outside_alias = self.root / 'source-alias.nes'
        outside_alias.symlink_to(inside)
        with self.assertRaisesRegex(kit.PrepareError, 'outside the generated import folders'):
            kit.prepare(outside_alias, gym_patch=self.gym_patch, output_root=self.output, rights_acknowledged=True)
        self.assertEqual(inside.read_bytes(), self.source)

    def test_output_links_unrelated_files_and_incompatible_layout_are_rejected(self):
        self.output.mkdir()
        unrelated = self.root / 'other'
        unrelated.mkdir()
        (unrelated / 'keep').write_text('original')
        output = self.output / 'tetris-rom'
        output.symlink_to(unrelated, target_is_directory=True)
        with self.assertRaisesRegex(kit.PrepareError, 'normal directory'):
            self.prepare()
        self.assertEqual((unrelated / 'keep').read_text(), 'original')
        output.unlink(); output.mkdir()
        note = output / 'unrelated.txt'
        note.write_text('keep')
        with self.assertRaisesRegex(kit.PrepareError, 'unrelated'):
            self.prepare()
        self.assertEqual(note.read_text(), 'keep')
        note.unlink(); (output / 'source').write_bytes(b'not a directory')
        with self.assertRaisesRegex(kit.PrepareError, 'incompatible file layout'):
            self.prepare()

    def test_staging_failure_preserves_imports_and_cleans_temporary_files(self):
        self.prepare()
        original = self.snapshot()
        generate = import_rom.generate
        calls = 0
        def failing_generate(*args, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError('synthetic full disk')
            return generate(*args, **kwargs)
        with mock.patch.object(import_rom, 'generate', side_effect=failing_generate):
            with self.assertRaisesRegex(OSError, 'synthetic full disk'):
                self.prepare()
        self.assertEqual(self.snapshot(), original)
        self.assertFalse(list(self.output.glob('.tetris-prepare-*')))

    def test_publish_failure_rolls_back_both_previous_imports(self):
        self.prepare()
        for folder in ('tetris-rom', 'tetris-zero-rom'):
            (self.output / folder / 'generated_rom.h').write_text('previous ' + folder)
        original = self.snapshot()
        replace = kit.os.replace
        failed = False
        def failing_replace(source, destination):
            nonlocal failed
            source, destination = Path(source), Path(destination)
            if not failed and source.name == 'tetris-zero-rom' and source.parent.name.startswith('.tetris-prepare-'):
                failed = True
                raise OSError('synthetic publish failure')
            return replace(source, destination)
        with mock.patch.object(kit.os, 'replace', side_effect=failing_replace):
            with self.assertRaisesRegex(OSError, 'synthetic publish failure'):
                self.prepare()
        self.assertTrue(failed)
        self.assertEqual(self.snapshot(), original)
        self.assertFalse(list(self.output.glob('.tetris-prepare-*')))

    def test_interrupt_during_publish_restores_previous_imports(self):
        self.prepare()
        original = self.snapshot()
        replace = kit.os.replace
        interrupted = False
        def interrupt_once(source, destination):
            nonlocal interrupted
            if not interrupted and Path(source).name == 'tetris-zero-rom' and Path(source).parent == self.output.resolve():
                interrupted = True
                raise KeyboardInterrupt()
            return replace(source, destination)
        with mock.patch.object(kit.os, 'replace', side_effect=interrupt_once):
            with self.assertRaises(KeyboardInterrupt):
                self.prepare()
        self.assertTrue(interrupted)
        self.assertEqual(self.snapshot(), original)
        self.assertFalse(list(self.output.glob('.tetris-prepare-*')))


if __name__ == '__main__':
    unittest.main()
