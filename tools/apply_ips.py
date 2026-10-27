#!/usr/bin/env python3
"""Apply a local IPS patch with bounded writes and optional source SHA-1 validation."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

MAX_BYTES = 1024 * 1024


class PatchError(ValueError):
    pass


def apply(source: bytes, patch: bytes, *, source_sha1: str | None = None) -> tuple[bytes, dict]:
    if not isinstance(source, bytes) or not isinstance(patch, bytes):
        raise PatchError('Source and patch inputs must be bytes')
    if len(source) > MAX_BYTES or len(patch) > MAX_BYTES:
        raise PatchError('Source and patch must each fit within 1 MiB')
    if len(patch) < 8 or patch[:5] != b'PATCH':
        raise PatchError('A complete IPS patch is required')
    actual_source_sha1 = hashlib.sha1(source).hexdigest()
    if source_sha1 is not None:
        if not isinstance(source_sha1, str) or not re.fullmatch(r'[0-9a-fA-F]{40}', source_sha1):
            raise PatchError('Expected source SHA-1 must contain 40 hexadecimal characters')
        source_sha1 = source_sha1.lower()
        if actual_source_sha1 != source_sha1:
            raise PatchError('Source SHA-1 does not match the expected value')
    output = bytearray(source)
    cursor = 5
    records = 0

    def take(count):
        nonlocal cursor
        if cursor + count > len(patch):
            raise PatchError('IPS instruction is truncated')
        data = patch[cursor:cursor + count]
        cursor += count
        return data

    while True:
        encoded_offset = take(3)
        if encoded_offset == b'EOF':
            break
        offset = int.from_bytes(encoded_offset, 'big')
        count = int.from_bytes(take(2), 'big')
        if count:
            if offset + count > MAX_BYTES:
                raise PatchError('IPS write exceeds the 1 MiB target limit')
            data = take(count)
        else:
            count = int.from_bytes(take(2), 'big')
            if not count:
                raise PatchError('IPS repeated-byte length must be positive')
            if offset + count > MAX_BYTES:
                raise PatchError('IPS write exceeds the 1 MiB target limit')
            data = take(1) * count
        end = offset + count
        if end > len(output):
            output.extend(bytes(end - len(output)))
        output[offset:end] = data
        records += 1

    remaining = len(patch) - cursor
    if remaining not in (0, 3):
        raise PatchError('IPS footer must end at EOF or contain one three-byte target size')
    resize = None
    if remaining:
        resize = int.from_bytes(take(3), 'big')
        if resize > MAX_BYTES:
            raise PatchError('IPS resize exceeds the 1 MiB target limit')
        if resize < len(output):
            del output[resize:]
        else:
            output.extend(bytes(resize - len(output)))
    result = bytes(output)
    return result, {
        'format': 1, 'patch_format': 'IPS', 'source_bytes': len(source),
        'source_sha1': actual_source_sha1, 'expected_source_sha1': source_sha1,
        'source_sha256': hashlib.sha256(source).hexdigest(),
        'patch_sha256': hashlib.sha256(patch).hexdigest(),
        'target_sha1': hashlib.sha1(result).hexdigest(),
        'target_sha256': hashlib.sha256(result).hexdigest(),
        'target_bytes': len(result), 'records': records, 'resize_bytes': resize,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True, help='Local source file')
    parser.add_argument('--patch', type=Path, required=True, help='Local IPS patch')
    parser.add_argument('--output', type=Path, required=True, help='Private patched output file')
    parser.add_argument('--source-sha1', help='Expected source SHA-1 from an independently verified reference')
    args = parser.parse_args(argv)
    receipt_path = args.output.with_suffix('.patch-receipt.json')

    def same_file(left, right):
        return left.resolve() == right.resolve() or (left.exists() and right.exists() and left.samefile(right))

    try:
        for destination in (args.output, receipt_path):
            if any(same_file(destination, original) for original in (args.rom, args.patch)):
                parser.error('Choose output and receipt paths separate from the original ROM and patch')
        if same_file(args.output, receipt_path):
            parser.error('Choose separate output and receipt paths')
        if args.output.is_dir() or receipt_path.is_dir():
            parser.error('Output and receipt paths must be files')
    except OSError as exc:
        parser.error('Output paths could not be checked: ' + str(exc))
    try:
        with args.rom.open('rb') as source_file:
            source = source_file.read(MAX_BYTES + 1)
        with args.patch.open('rb') as patch_file:
            patch = patch_file.read(MAX_BYTES + 1)
        result, receipt = apply(source, patch, source_sha1=args.source_sha1)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(result)
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    except (OSError, PatchError) as exc:
        parser.exit(2, f'Patch failed: {exc}\n')
    print('Patched file and hash receipt written.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
