#!/usr/bin/env python3
"""Apply a local BPS patch with source, target and patch checksum validation."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

MAX_BYTES = 1024 * 1024


class PatchError(ValueError):
    pass


def apply(source: bytes, patch: bytes, *, nes_mirroring_header=False) -> tuple[bytes, dict]:
    if len(source) > MAX_BYTES or len(patch) > MAX_BYTES:
        raise PatchError('Source and patch must each fit within 1 MiB')
    if len(patch) < 19 or patch[:4] != b'BPS1':
        raise PatchError('A complete BPS1 patch is required')
    source_crc, target_crc, patch_crc = struct.unpack('<III', patch[-12:])
    if zlib.crc32(patch[:-4]) != patch_crc:
        raise PatchError('Patch checksum does not match')
    end = len(patch) - 12
    cursor = 4

    def number():
        nonlocal cursor
        value, weight = 0, 1
        for _ in range(8):
            if cursor >= end:
                raise PatchError('Patch instruction is truncated')
            byte = patch[cursor]
            cursor += 1
            value += (byte & 127) * weight
            if byte & 128:
                return value
            weight <<= 7
            value += weight
        raise PatchError('Patch number exceeds the size limit')

    source_size, target_size, metadata_size = number(), number(), number()
    if source_size != len(source) or target_size > MAX_BYTES:
        raise PatchError('Source length or target size does not match the patch limits')
    if cursor + metadata_size > end:
        raise PatchError('Patch metadata is truncated')
    cursor += metadata_size
    original = source
    normalization = None
    if zlib.crc32(source) != source_crc and nes_mirroring_header and source[:4] == b'NES\x1a' and len(source) >= 16:
        candidate = source[:6] + bytes([source[6] ^ 1]) + source[7:]
        if zlib.crc32(candidate) == source_crc:
            source = candidate
            normalization = {'header_byte': 6, 'original': original[6], 'patched_source': source[6]}
    if zlib.crc32(source) != source_crc:
        raise PatchError('Source checksum does not match this patch')
    output = bytearray()
    relative = [0, 0]
    while cursor < end:
        command = number()
        mode, count = command & 3, (command >> 2) + 1
        if len(output) + count > target_size:
            raise PatchError('Patch instruction exceeds the target length')
        if mode == 0:
            position = len(output)
            if position + count > len(source):
                raise PatchError('Source read exceeds the source length')
            output.extend(source[position:position + count])
        elif mode == 1:
            if cursor + count > end:
                raise PatchError('Literal data is truncated')
            output.extend(patch[cursor:cursor + count])
            cursor += count
        else:
            displacement = number()
            index = mode - 2
            relative[index] += -(displacement >> 1) if displacement & 1 else displacement >> 1
            position = relative[index]
            if position < 0:
                raise PatchError('Copy starts before the buffer')
            if mode == 2:
                if position + count > len(source):
                    raise PatchError('Source copy exceeds the source length')
                output.extend(source[position:position + count])
            else:
                if position >= len(output):
                    raise PatchError('Target copy requires already produced data')
                for offset in range(count):
                    output.append(output[position + offset])
            relative[index] += count
    if len(output) != target_size or zlib.crc32(output) != target_crc:
        raise PatchError('Patched target length or checksum does not match')
    result = bytes(output)
    return result, {'format': 1, 'source_sha256': hashlib.sha256(original).hexdigest(),
                    'patch_sha256': hashlib.sha256(patch).hexdigest(),
                    'target_sha256': hashlib.sha256(result).hexdigest(),
                    'source_crc32': f'{source_crc:08x}', 'target_crc32': f'{target_crc:08x}',
                    'target_bytes': len(result), 'nes_header_normalization': normalization}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--patch', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--nes-mirroring-header', action='store_true',
                        help='Allow flipping only the NES mirroring header bit when it matches the exact source checksum')
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
        with args.rom.open('rb') as f:
            source = f.read(MAX_BYTES + 1)
        with args.patch.open('rb') as f:
            patch = f.read(MAX_BYTES + 1)
        result, receipt = apply(source, patch, nes_mirroring_header=args.nes_mirroring_header)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(result)
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    except (OSError, PatchError) as exc:
        parser.exit(2, f'Patch failed: {exc}\n')
    print('Patched ROM written. All three BPS checksums verified.')


if __name__ == '__main__':
    main()
