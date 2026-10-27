"""Check IPS parsing and local file preservation with synthetic bytes."""
from __future__ import annotations

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import apply_ips as ips


def literal(offset, data):
    return offset.to_bytes(3, 'big') + len(data).to_bytes(2, 'big') + data


def repeated(offset, count, value):
    return offset.to_bytes(3, 'big') + bytes(2) + count.to_bytes(2, 'big') + bytes([value])


def patch(*records, resize=None):
    result = b'PATCH' + b''.join(records) + b'EOF'
    return result if resize is None else result + resize.to_bytes(3, 'big')


class IpsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='m5-ips-test-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def test_literal_overwrites_preserves_other_bytes_and_zero_fills_gaps(self):
        source = b'abcdefgh'
        encoded = patch(literal(2, b'XY'), literal(10, b'Z'))
        result, receipt = ips.apply(source, encoded)
        self.assertEqual(result, b'abXYefgh\0\0Z')
        self.assertEqual(source, b'abcdefgh')
        self.assertEqual(receipt['records'], 2)
        self.assertIsNone(receipt['resize_bytes'])

    def test_repeated_bytes_and_overlapping_records_follow_file_order(self):
        result, _ = ips.apply(b'abcdef', patch(repeated(1, 5, ord('X')), literal(3, b'yz'),
                                                repeated(4, 4, ord('Q'))))
        self.assertEqual(result, b'aXXyQQQQ')
        self.assertEqual(ips.apply(b'', patch(repeated(0, 65535, 42)))[0], b'*' * 65535)

    def test_empty_patch_and_optional_resize_shrink_extend_or_empty(self):
        source = b'original'
        self.assertEqual(ips.apply(source, patch())[0], source)
        self.assertEqual(ips.apply(b'', patch())[0], b'')
        for size, expected in ((3, b'ori'), (8, source), (10, source + bytes(2)), (0, b'')):
            with self.subTest(size=size):
                result, receipt = ips.apply(source, patch(resize=size))
                self.assertEqual(result, expected)
                self.assertEqual(receipt['resize_bytes'], size)
        self.assertEqual(ips.apply(source, patch(literal(7, b'XYZ'), resize=9))[0], b'originaXY')

    def test_hash_receipt_and_optional_case_insensitive_source_sha1(self):
        source, target = b'original', b'fixednal'
        encoded = patch(literal(0, b'fixed'))
        expected_sha1 = hashlib.sha1(source).hexdigest()
        result, receipt = ips.apply(source, encoded, source_sha1=expected_sha1.upper())
        self.assertEqual(result, target)
        self.assertEqual(receipt['patch_format'], 'IPS')
        self.assertEqual(receipt['source_bytes'], len(source))
        self.assertEqual(receipt['target_bytes'], len(target))
        self.assertEqual(receipt['source_sha1'], expected_sha1)
        self.assertEqual(receipt['expected_source_sha1'], expected_sha1)
        self.assertEqual(receipt['target_sha1'], hashlib.sha1(target).hexdigest())
        for key, data in (('source', source), ('patch', encoded), ('target', target)):
            self.assertEqual(receipt[key + '_sha256'], hashlib.sha256(data).hexdigest())
        self.assertIsNone(ips.apply(source, encoded)[1]['expected_source_sha1'])
        with self.assertRaisesRegex(ips.PatchError, 'does not match'):
            ips.apply(source, encoded, source_sha1='0' * 40)
        for invalid in ('', 'f' * 39, 'f' * 41, 'g' * 40, ' ' + expected_sha1, 123):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(ips.PatchError, '40 hexadecimal'):
                ips.apply(source, encoded, source_sha1=invalid)

    def test_every_truncated_instruction_prefix_is_rejected(self):
        for encoded in (patch(literal(0, b'fixture')), patch(repeated(3, 7, 42)),
                        patch(literal(1, b'abcd'), repeated(12, 7, 42))):
            for length in range(len(encoded)):
                with self.subTest(length=length, total=len(encoded)), self.assertRaises(ips.PatchError):
                    ips.apply(b'source', encoded[:length])

    def test_bad_magic_zero_rle_length_and_trailing_bytes_are_rejected(self):
        for encoded, message in ((b'WRONGEOF', 'complete IPS'),
                                  (patch(repeated(0, 0, 42)), 'positive'),
                                  (patch() + b'x', 'footer'), (patch() + b'xy', 'footer'),
                                  (patch() + bytes(4), 'footer'),
                                  (patch() + bytes(3) + b'EOF', 'footer')):
            with self.subTest(message=message), self.assertRaisesRegex(ips.PatchError, message):
                ips.apply(b'base', encoded)
        for source, encoded in ((None, patch()), (bytearray(), patch()), (b'', bytearray(patch())), (b'', None)):
            with self.assertRaisesRegex(ips.PatchError, 'must be bytes'):
                ips.apply(source, encoded)

    def test_exact_one_mib_limits_and_no_oversized_intermediate_target(self):
        source = b'S' * ips.MAX_BYTES
        self.assertEqual(ips.apply(source, patch())[0], source)
        result, _ = ips.apply(b'', patch(literal(ips.MAX_BYTES - 1, b'Z')))
        self.assertEqual(len(result), ips.MAX_BYTES)
        self.assertEqual(result[:-1], bytes(ips.MAX_BYTES - 1))
        self.assertEqual(result[-1:], b'Z')
        self.assertEqual(ips.apply(b'', patch(repeated(ips.MAX_BYTES - 1, 1, 42)))[0][-1:], b'*')
        self.assertEqual(len(ips.apply(b'', patch(resize=ips.MAX_BYTES))[0]), ips.MAX_BYTES)
        for source, encoded in ((bytes(ips.MAX_BYTES + 1), patch()), (b'', bytes(ips.MAX_BYTES + 1)),
                                 (b'', patch(literal(ips.MAX_BYTES, b'X'))),
                                 (b'', patch(repeated(ips.MAX_BYTES - 1, 2, 42))),
                                 (b'', patch(resize=ips.MAX_BYTES + 1)),
                                 (b'', patch(literal(ips.MAX_BYTES, b'X'), resize=1)),
                                 (b'', patch(literal(0xffffff, b'X')))):
            with self.subTest(source_bytes=len(source), patch_bytes=len(encoded)), \
                    self.assertRaisesRegex(ips.PatchError, '1 MiB'):
                ips.apply(source, encoded)

    def make_inputs(self):
        source = self.directory / 'source.nes'
        encoded = self.directory / 'change.ips'
        source.write_bytes(b'synthetic source')
        encoded.write_bytes(patch(literal(0, b'patched')))
        return source, encoded

    def invoke(self, source, encoded, output, *extra):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return ips.main(['--rom', str(source), '--patch', str(encoded), '--output', str(output), *extra])

    def test_cli_writes_output_and_receipt_without_changing_originals(self):
        source, encoded = self.make_inputs()
        source_before, patch_before = source.read_bytes(), encoded.read_bytes()
        output = self.directory / 'private' / 'patched.nes'
        self.assertEqual(self.invoke(source, encoded, output, '--source-sha1',
                                     hashlib.sha1(source_before).hexdigest()), 0)
        self.assertEqual(output.read_bytes(), b'patchedic source')
        receipt = json.loads(output.with_suffix('.patch-receipt.json').read_text())
        self.assertEqual(receipt['target_sha256'], hashlib.sha256(output.read_bytes()).hexdigest())
        self.assertEqual(receipt['expected_source_sha1'], hashlib.sha1(source_before).hexdigest())
        self.assertEqual(source.read_bytes(), source_before)
        self.assertEqual(encoded.read_bytes(), patch_before)

    def test_cli_validation_failure_writes_nothing_and_preserves_existing_output(self):
        source, encoded = self.make_inputs()
        output = self.directory / 'not-created' / 'patched.nes'
        for content, extra in ((b'bad', ()), (patch(literal(0, b'X')), ('--source-sha1', '0' * 40)),
                               (bytes(ips.MAX_BYTES + 1), ())):
            encoded.write_bytes(content)
            with self.assertRaises(SystemExit) as raised:
                self.invoke(source, encoded, output, *extra)
            self.assertEqual(raised.exception.code, 2)
            self.assertFalse(output.parent.exists())
        output.parent.mkdir()
        output.write_bytes(b'existing result')
        receipt = output.with_suffix('.patch-receipt.json')
        receipt.write_bytes(b'existing receipt')
        with self.assertRaises(SystemExit):
            self.invoke(source, encoded, output)
        self.assertEqual(output.read_bytes(), b'existing result')
        self.assertEqual(receipt.read_bytes(), b'existing receipt')
        self.assertEqual(source.read_bytes(), b'synthetic source')

    def test_cli_rejects_direct_output_and_receipt_input_collisions(self):
        source, encoded = self.make_inputs()
        source_before, patch_before = source.read_bytes(), encoded.read_bytes()
        for output in (source, encoded):
            with self.assertRaises(SystemExit):
                self.invoke(source, encoded, output)
        receipt_source = self.directory / 'result.patch-receipt.json'
        receipt_source.write_bytes(source_before)
        output = self.directory / 'result.nes'
        with self.assertRaises(SystemExit):
            self.invoke(receipt_source, encoded, output)
        self.assertFalse(output.exists())
        self.assertEqual(receipt_source.read_bytes(), source_before)
        self.assertEqual(source.read_bytes(), source_before)
        self.assertEqual(encoded.read_bytes(), patch_before)

    def test_cli_rejects_symlink_hardlink_and_directory_destinations(self):
        source, encoded = self.make_inputs()
        source_before, patch_before = source.read_bytes(), encoded.read_bytes()
        output = self.directory / 'result.nes'
        receipt = output.with_suffix('.patch-receipt.json')
        for alias in ('output-hardlink', 'output-symlink', 'receipt-hardlink', 'receipt-symlink',
                      'output-directory', 'receipt-directory', 'receipt-output-hardlink'):
            with self.subTest(alias=alias):
                if alias == 'output-hardlink': os.link(source, output)
                elif alias == 'output-symlink': output.symlink_to(source)
                elif alias == 'receipt-hardlink': os.link(encoded, receipt)
                elif alias == 'receipt-symlink': receipt.symlink_to(encoded)
                elif alias == 'output-directory': output.mkdir()
                elif alias == 'receipt-directory': receipt.mkdir()
                else:
                    output.write_bytes(b'existing output')
                    os.link(output, receipt)
                with self.assertRaises(SystemExit) as raised:
                    self.invoke(source, encoded, output)
                self.assertEqual(raised.exception.code, 2)
                self.assertEqual(source.read_bytes(), source_before)
                self.assertEqual(encoded.read_bytes(), patch_before)
                for destination in (output, receipt):
                    if destination.is_symlink() or destination.is_file(): destination.unlink()
                    elif destination.is_dir(): destination.rmdir()


if __name__ == '__main__':
    unittest.main()
