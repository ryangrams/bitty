"""Exercise BPS instructions and checksums using synthetic source bytes."""
from __future__ import annotations

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import apply_bps as bps


def integer(value):
    encoded = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        if not value:
            encoded.append(byte | 128)
            return bytes(encoded)
        encoded.append(byte)
        value -= 1


def action(mode, count, argument=None):
    encoded = integer(((count - 1) << 2) | mode)
    if mode == 1:
        return encoded + argument
    if mode in (2, 3):
        return encoded + integer(abs(argument) * 2 + (argument < 0))
    return encoded


def footer(body, source, target, *, source_crc=None, target_crc=None):
    body += struct.pack('<II', zlib.crc32(source) if source_crc is None else source_crc,
                        zlib.crc32(target) if target_crc is None else target_crc)
    return body + struct.pack('<I', zlib.crc32(body))


def patch(source, target, instructions, *, metadata=b'', source_size=None, target_size=None,
          source_crc=None, target_crc=None):
    body = b'BPS1' + integer(len(source) if source_size is None else source_size)
    body += integer(len(target) if target_size is None else target_size) + integer(len(metadata))
    return footer(body + metadata + instructions, source, target, source_crc=source_crc, target_crc=target_crc)


class BpsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='m5-bps-test-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def test_all_four_actions_and_negative_source_copy(self):
        source = b'abcdefghij'
        target = b'abXYfghbcabX'
        instructions = action(0, 2) + action(1, 2, b'XY')
        instructions += action(2, 3, 5) + action(2, 2, -7) + action(3, 3, 0)
        encoded = patch(source, target, instructions, metadata=b'Synthetic fixture metadata')
        result, receipt = bps.apply(source, encoded)
        self.assertEqual(result, target)
        self.assertEqual(receipt['source_sha256'], hashlib.sha256(source).hexdigest())
        self.assertEqual(receipt['patch_sha256'], hashlib.sha256(encoded).hexdigest())
        self.assertEqual(receipt['target_sha256'], hashlib.sha256(target).hexdigest())
        self.assertEqual(receipt['source_crc32'], f'{zlib.crc32(source):08x}')
        self.assertEqual(receipt['target_crc32'], f'{zlib.crc32(target):08x}')
        self.assertEqual(receipt['target_bytes'], len(target))
        self.assertIsNone(receipt['nes_header_normalization'])

    def test_overlapping_target_copy_expands_just_written_bytes(self):
        target = b'AB' * 16
        encoded = patch(b'', target, action(1, 2, b'AB') + action(3, len(target) - 2, 0))
        self.assertEqual(bps.apply(b'', encoded)[0], target)
        target = b'abcdbcab' + b'b' * 8
        instructions = action(1, 4, b'abcd') + action(3, 2, 1) + action(3, 2, -3) + action(3, 8, 5)
        self.assertEqual(bps.apply(b'', patch(b'', target, instructions))[0], target)

    def test_empty_patch_target_and_exact_one_mib_limits(self):
        self.assertEqual(bps.apply(b'', patch(b'', b'', b''))[0], b'')
        source = b'S' * bps.MAX_BYTES
        self.assertEqual(bps.apply(source, patch(source, source, action(0, len(source))))[0], source)
        target = b'T' * bps.MAX_BYTES
        instructions = action(1, 1, b'T') + action(3, len(target) - 1, 0)
        self.assertEqual(bps.apply(b'', patch(b'', target, instructions))[0], target)
        with self.assertRaisesRegex(bps.PatchError, '1 MiB'):
            bps.apply(bytes(bps.MAX_BYTES + 1), patch(b'', b'', b''))
        with self.assertRaisesRegex(bps.PatchError, '1 MiB'):
            bps.apply(b'', bytes(bps.MAX_BYTES + 1))
        with self.assertRaisesRegex(bps.PatchError, 'limits'):
            bps.apply(b'', patch(b'', b'', b'', target_size=bps.MAX_BYTES + 1))

    def test_source_target_and_patch_checksums_are_independent(self):
        source, target = b'SOURCE', b'TARGET'
        instructions = action(1, len(target), target)
        encoded = patch(source, target, instructions)
        with self.assertRaisesRegex(bps.PatchError, 'Source checksum'):
            bps.apply(b'WRONG!', encoded)
        with self.assertRaisesRegex(bps.PatchError, 'Source checksum'):
            bps.apply(source, patch(source, target, instructions, source_crc=1))
        with self.assertRaisesRegex(bps.PatchError, 'target length or checksum'):
            bps.apply(source, patch(source, target, instructions, target_crc=1))
        broken = bytearray(encoded)
        broken[-1] ^= 1
        with self.assertRaisesRegex(bps.PatchError, 'Patch checksum'):
            bps.apply(source, bytes(broken))
        broken = bytearray(encoded)
        broken[9] ^= 1
        with self.assertRaisesRegex(bps.PatchError, 'Patch checksum'):
            bps.apply(source, bytes(broken))

    def test_every_truncated_patch_prefix_is_rejected(self):
        source, target = b'source', b'target literal'
        encoded = patch(source, target, action(1, len(target), target), metadata=b'metadata')
        for length in range(len(encoded)):
            with self.subTest(length=length), self.assertRaises(bps.PatchError):
                bps.apply(source, encoded[:length])
        with self.assertRaisesRegex(bps.PatchError, 'BPS1'):
            bps.apply(source, b'WRONG' + encoded[5:])

    def test_checked_instruction_bounds_and_oversized_varints(self):
        cases = [
            (b'', b'', footer(b'BPS1' + b'\x00' * 9, b'', b''), 'number exceeds'),
            (b'', b'', footer(b'BPS1' + integer(0) + integer(0) + integer(5) + b'ab', b'', b''), 'metadata is truncated'),
            (b'a', b'a', patch(b'a', b'a', b'\x00'), 'instruction is truncated'),
            (b'a', b'ab', patch(b'a', b'ab', action(1, 2, b'a')), 'Literal data is truncated'),
            (b'a', b'ab', patch(b'a', b'ab', action(0, 2)), 'Source read exceeds'),
            (b'a', b'b', patch(b'a', b'b', action(2, 1, -1)), 'before the buffer'),
            (b'a', b'b', patch(b'a', b'b', action(2, 1, 1)), 'Source copy exceeds'),
            (b'', b'a', patch(b'', b'a', action(3, 1, 0)), 'already produced'),
            (b'', b'a', patch(b'', b'a', action(3, 1, -1)), 'before the buffer'),
            (b'', b'ab', patch(b'', b'ab', action(1, 1, b'a') + action(3, 1, 1)), 'already produced'),
            (b'a', b'a', patch(b'a', b'a', action(0, 2)), 'target length'),
            (b'a', b'a', patch(b'a', b'a', b''), 'target length or checksum'),
            (b'a', b'a', patch(b'a', b'a', action(0, 1), source_size=2), 'Source length'),
            (b'a', b'a', patch(b'a', b'a', action(0, 1) + action(0, 1)), 'target length'),
        ]
        for source, target, encoded, error in cases:
            with self.subTest(error=error), self.assertRaisesRegex(bps.PatchError, error):
                bps.apply(source, encoded)

    def test_nes_mirroring_normalization_requires_exact_crc_and_preserves_other_bytes(self):
        source = b'NES\x1a' + bytes([1, 1, 0x20]) + bytes(9) + b'original payload'
        normalized = source[:6] + bytes([source[6] ^ 1]) + source[7:]
        encoded = patch(normalized, normalized, action(0, len(normalized)))
        with self.assertRaisesRegex(bps.PatchError, 'Source checksum'):
            bps.apply(source, encoded)
        result, receipt = bps.apply(source, encoded, nes_mirroring_header=True)
        self.assertEqual(result, normalized)
        self.assertEqual(result[:6], source[:6])
        self.assertEqual(result[7:], source[7:])
        self.assertEqual(receipt['source_sha256'], hashlib.sha256(source).hexdigest())
        self.assertEqual(receipt['nes_header_normalization'], {'header_byte': 6, 'original': 32, 'patched_source': 33})
        unchanged = patch(source, source, action(0, len(source)))
        self.assertIsNone(bps.apply(source, unchanged, nes_mirroring_header=True)[1]['nes_header_normalization'])
        for offset, bit in ((6, 2), (7, 0x10), (16, 1)):
            other = bytearray(source)
            other[offset] ^= bit
            with self.assertRaisesRegex(bps.PatchError, 'Source checksum'):
                bps.apply(source, patch(bytes(other), bytes(other), action(0, len(other))), nes_mirroring_header=True)
        plain = b'not NES!' + bytes(24)
        other = plain[:6] + bytes([plain[6] ^ 1]) + plain[7:]
        with self.assertRaisesRegex(bps.PatchError, 'Source checksum'):
            bps.apply(plain, patch(other, other, action(0, len(other))), nes_mirroring_header=True)

    def run_cli(self, rom, encoded, output):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return bps.main(['--rom', str(rom), '--patch', str(encoded), '--output', str(output)])

    def test_cli_writes_target_receipt_and_preserves_original_inputs(self):
        source, target = b'original', b'patched'
        rom, encoded, output = self.directory / 'original.nes', self.directory / 'change.bps', self.directory / 'private' / 'patched.nes'
        rom.write_bytes(source)
        original_patch = patch(source, target, action(1, len(target), target))
        encoded.write_bytes(original_patch)
        self.assertIsNone(self.run_cli(rom, encoded, output))
        self.assertEqual(output.read_bytes(), target)
        self.assertEqual(rom.read_bytes(), source)
        self.assertEqual(encoded.read_bytes(), original_patch)
        receipt = json.loads(output.with_suffix('.patch-receipt.json').read_text())
        self.assertEqual(receipt['target_sha256'], hashlib.sha256(target).hexdigest())

    def test_cli_bad_patch_writes_nothing_and_preserves_existing_output(self):
        rom, encoded, output = self.directory / 'original.nes', self.directory / 'bad.bps', self.directory / 'private' / 'patched.nes'
        rom.write_bytes(b'original')
        encoded.write_bytes(b'not a patch')
        with self.assertRaises(SystemExit) as raised:
            self.run_cli(rom, encoded, output)
        self.assertEqual(raised.exception.code, 2)
        self.assertFalse(output.parent.exists())
        output.parent.mkdir()
        output.write_bytes(b'keep existing')
        with self.assertRaises(SystemExit):
            self.run_cli(rom, encoded, output)
        self.assertEqual(output.read_bytes(), b'keep existing')
        self.assertFalse(output.with_suffix('.patch-receipt.json').exists())
        self.assertEqual(rom.read_bytes(), b'original')
        self.assertEqual(encoded.read_bytes(), b'not a patch')

    def test_cli_rejects_output_and_receipt_aliases_including_hardlinks(self):
        source = b'original'
        rom, encoded = self.directory / 'original.nes', self.directory / 'change.bps'
        rom.write_bytes(source)
        original_patch = patch(source, source, action(0, len(source)))
        encoded.write_bytes(original_patch)
        for output in (rom, encoded):
            with self.subTest(output=output), self.assertRaises(SystemExit):
                self.run_cli(rom, encoded, output)
        alias = self.directory / 'alias.nes'
        os.link(rom, alias)
        with self.assertRaises(SystemExit):
            self.run_cli(rom, encoded, alias)
        alias.unlink()
        alias.symlink_to(rom)
        with self.assertRaises(SystemExit):
            self.run_cli(rom, encoded, alias)
        collision = self.directory / 'collision.nes'
        receipt = collision.with_suffix('.patch-receipt.json')
        receipt.symlink_to(encoded)
        with self.assertRaises(SystemExit):
            self.run_cli(rom, encoded, collision)
        receipt.unlink()
        receipt.write_bytes(source)
        with self.assertRaises(SystemExit):
            self.run_cli(receipt, encoded, collision)
        self.assertEqual(receipt.read_bytes(), source)
        self.assertEqual(rom.read_bytes(), source)
        self.assertEqual(encoded.read_bytes(), original_patch)
        self.assertFalse(collision.exists())


if __name__ == '__main__':
    unittest.main()
