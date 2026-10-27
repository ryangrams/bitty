#!/usr/bin/env python3
"""Convert a local NES cartridge image into a private firmware header."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / '.local' / 'nes-rom'
MAX_ROM_BYTES = 1024 * 1024
MAX_TITLE_BYTES = 40
SUPPORTED_MAPPERS = (0, 37)
RIGHTS_CONFIRMATION = ('I confirm I have the rights or permission needed to use and convert '
                       'the ROM I supply for this build.')


class RomError(ValueError):
    """A local cartridge cannot be imported by the supported NES core."""


def clean_title(title: str) -> str:
    if not isinstance(title, str):
        raise RomError('ROM title must be text')
    if any(not char.isprintable() or unicodedata.category(char).startswith('C') for char in title):
        raise RomError('ROM title must contain 1 to 40 UTF-8 bytes without control characters')
    title = title.strip()
    try:
        encoded = title.encode('utf-8')
    except UnicodeError as exc:
        raise RomError('ROM title must contain valid UTF-8 text') from exc
    if not encoded or len(encoded) > MAX_TITLE_BYTES:
        raise RomError('ROM title must contain 1 to 40 UTF-8 bytes without control characters')
    return title


def cpp_string(value: str) -> str:
    characters = []
    for byte in value.encode('utf-8'):
        if byte in (34, 92):
            characters.append('\\' + chr(byte))
        elif 32 <= byte <= 126:
            characters.append(chr(byte))
        else:
            characters.append(f'\\{byte:03o}')
    return '"' + ''.join(characters) + '"'


def inspect_rom(rom: bytes) -> dict:
    if not isinstance(rom, bytes):
        raise RomError('ROM input must be bytes')
    if len(rom) > MAX_ROM_BYTES:
        raise RomError('ROM exceeds the 1 MiB size limit')
    if len(rom) < 16:
        raise RomError('ROM header is incomplete')
    header = rom[:16]
    if header[:4] != b'NES\x1a':
        raise RomError('ROM requires an iNES or NES 2.0 header')
    identifier = header[7] & 0x0c
    if identifier not in (0, 8):
        raise RomError('ROM header format is unsupported')
    nes2 = identifier == 8
    if header[6] & 4:
        raise RomError('ROM trainers are unsupported')
    if header[7] & 3:
        raise RomError('Only standard NES console cartridges are supported')
    mapper = (header[6] >> 4) | (header[7] & 0xf0)
    submapper = 0
    prg_units, chr_units = header[4], header[5]
    if nes2:
        mapper |= (header[8] & 15) << 8
        submapper = header[8] >> 4
        if submapper:
            raise RomError('NES 2.0 submappers are unsupported')
        if (header[9] & 15) == 15 or (header[9] >> 4) == 15:
            raise RomError('NES 2.0 exponent ROM sizes are unsupported')
        prg_units |= (header[9] & 15) << 8
        chr_units |= (header[9] >> 4) << 8
        if header[12] & 0xfc or header[14] & 0xfc or header[15] & 0xc0:
            raise RomError('NES 2.0 reserved header bits must be zero')
        timing = header[12] & 3
        if timing == 3:
            raise RomError('Dendy timing is unsupported')
        if header[13]:
            raise RomError('NES 2.0 console extensions are unsupported')
        if header[14] & 3:
            raise RomError('Miscellaneous ROM sections are unsupported')
    else:
        if header[9] & 0xfe or any(header[10:16]):
            raise RomError('iNES reserved header bytes must be zero')
        if header[8] > 1:
            raise RomError('iNES PRG RAM larger than 8 KiB is unsupported')
        timing = header[9] & 1
    if mapper not in SUPPORTED_MAPPERS:
        raise RomError(f'Unsupported mapper {mapper}; supported mappers are 0 and 37')
    if nes2 and header[10]:
        raise RomError('Explicit NES 2.0 PRG RAM and NVRAM declarations are unsupported')
    if nes2 and header[11] and not (mapper == 0 and chr_units == 0 and header[11] == 7):
        raise RomError('Only mapper 0 with 8 KiB volatile CHR RAM is supported')
    prg_bytes, chr_bytes = prg_units * 16384, chr_units * 8192
    expected = 16 + prg_bytes + chr_bytes
    if expected > MAX_ROM_BYTES:
        raise RomError('Declared ROM size exceeds the 1 MiB size limit')
    if len(rom) != expected:
        raise RomError(f'ROM length must match its header exactly: expected {expected} bytes, got {len(rom)}')
    if mapper == 0 and (prg_units not in (1, 2) or chr_units not in (0, 1)):
        raise RomError('Mapper 0 requires 16 or 32 KiB PRG and 0 or 8 KiB CHR')
    if mapper == 37 and (prg_units != 16 or chr_units != 32):
        raise RomError('Mapper 37 requires 256 KiB PRG and 256 KiB CHR')
    return {
        'rom_format': 'NES 2.0' if nes2 else 'iNES', 'mapper': mapper, 'submapper': submapper,
        'prg_bytes': prg_bytes, 'chr_bytes': chr_bytes, 'bytes': len(rom),
        'timing': ('NTSC', 'PAL', 'Multi-region')[timing], 'frame_rate': 50 if timing == 1 else 60,
        'battery': bool(header[6] & 2), 'four_screen': bool(header[6] & 8),
        'prg_ram_header': header[10] if nes2 else header[8],
        'chr_ram_header': header[11] if nes2 else 0,
        'expansion_device': header[15] & 63 if nes2 else 0,
    }


def rom_header(rom: bytes | None, title: str = 'NES') -> str:
    lines = ['// Generated from a local ROM. Keep this header private.', '#pragma once',
             '#include <stddef.h>', '#include <stdint.h>', '#ifdef ARDUINO',
             '#include <pgmspace.h>', '#else', '#ifndef PROGMEM', '#define PROGMEM',
             '#endif', '#endif', 'namespace FlappyRom {']
    if rom is None:
        lines.extend(['static const uint8_t* const data = nullptr;', 'static constexpr size_t length = 0;'])
    else:
        lines.append('alignas(4) static const uint8_t data[] PROGMEM = {')
        for offset in range(0, len(rom), 24):
            lines.append('  ' + ','.join(f'0x{value:02X}' for value in rom[offset:offset + 24]) + ',')
        lines.extend(['};', 'static constexpr size_t length = sizeof(data);'])
    lines.extend(['static const char title[] = ' + cpp_string(title) + ';', '}  // namespace FlappyRom'])
    return '\n'.join(lines) + '\n'


def generate(rom: bytes | None, output: Path, title: str = 'NES',
             rights_acknowledged: bool = False) -> dict:
    if rom is not None and not rights_acknowledged:
        raise RomError('Rights acknowledgment is required: ' + RIGHTS_CONFIRMATION)
    title = clean_title(title)
    metadata = inspect_rom(rom) if rom is not None else {'bytes': 0}
    receipt = {'format': 1, 'title': title, 'rightsAcknowledged': bool(rights_acknowledged),
               'rom_sha256': hashlib.sha256(rom).hexdigest() if rom is not None else None, **metadata}
    header = rom_header(rom, title)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    if rom is not None:
        source = output / 'source'
        source.mkdir(parents=True, exist_ok=True)
        (source / 'rom.nes').write_bytes(rom)
    (output / 'generated_rom.h').write_text(header, encoding='utf-8')
    (output / 'import_receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return receipt


def local_rom_path(value: str) -> Path:
    if not isinstance(value, str) or not value or '\x00' in value:
        raise RomError('ROM input requires a local .nes file')
    windows_drive = len(value) >= 3 and value[1] == ':' and value[2] in '/\\'
    if not windows_drive and re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:', value):
        raise RomError('ROM URLs are unsupported; choose a local .nes file')
    path = Path(value).expanduser()
    if path.suffix.lower() != '.nes':
        raise RomError('ROM input must be a local .nes file')
    if not path.is_file():
        raise RomError('Local ROM file was not found')
    return path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', help='Local .nes cartridge image')
    parser.add_argument('--title', default='NES', help='Menu title, at most 40 UTF-8 bytes')
    parser.add_argument('--output-dir', type=Path, default=DEFAULT_OUTPUT, help='Private output directory')
    parser.add_argument('--acknowledge-rights', action='store_true', help=RIGHTS_CONFIRMATION)
    args = parser.parse_args(argv)
    if args.rom is not None and not args.acknowledge_rights:
        parser.error('--acknowledge-rights is required when importing a ROM: ' + RIGHTS_CONFIRMATION)
    try:
        title = clean_title(args.title)
        data = None
        if args.rom is not None:
            path = local_rom_path(args.rom)
            with path.open('rb') as source:
                data = source.read(MAX_ROM_BYTES + 1)
        receipt = generate(data, args.output_dir, title, rights_acknowledged=args.acknowledge_rights)
    except (RomError, OSError) as exc:
        parser.exit(2, f'ROM import failed: {exc}\n')
    print(f"Private ROM header ready: {receipt['bytes']} bytes, title {receipt['title']}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
