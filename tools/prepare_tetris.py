#!/usr/bin/env python3
"""Prepare the supported private Tetris images from a local ROM and local patches."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import zlib

import apply_bps
import apply_ips
import import_rom

ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = import_rom.MAX_ROM_BYTES
ENGINE_VERSIONS = {
    'gym': {'title': 'TetrisGYM', 'version': '6.0.0', 'crc32': '37a5ec63',
            'mapper': 1, 'folder': 'tetris-rom'},
    'zero': {'title': 'Tetris Zero', 'version': 'MMC3_118_v2', 'crc32': '66462642',
             'mapper': 118, 'folder': 'tetris-zero-rom'},
}
OWNED_PATHS = {'generated_rom.h', 'import_receipt.json', 'patch_receipt.json',
               'preparation_receipt.json', 'source', 'source/rom.nes'}


class PrepareError(ValueError):
    """Local Tetris inputs could not be prepared safely."""


def _local_path(value, suffix=None) -> Path:
    text = str(value)
    windows_drive = len(text) >= 3 and text[1] == ':' and text[2] in '/\\'
    if not text or '\0' in text or (not windows_drive and re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:', text)):
        raise PrepareError('Choose local files and a local output folder')
    path = Path(text).expanduser()
    if suffix and path.suffix.lower() != suffix:
        raise PrepareError(f'Choose a local {suffix} file')
    return path


def _read(path: Path) -> bytes:
    if not path.is_file():
        raise PrepareError(f'Local input file was not found: {path.name}')
    with path.open('rb') as stream:
        data = stream.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise PrepareError(f'Input exceeds the 1 MiB limit: {path.name}')
    return data


def _identity(data: bytes) -> dict:
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'crc32': f'{zlib.crc32(data):08x}'}


def _inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _check_outputs(output_root: Path, engines: list[dict], inputs: list[Path]) -> None:
    if output_root.exists() and not output_root.is_dir():
        raise PrepareError('The output root must be a directory')
    for engine in engines:
        destination = output_root / engine['folder']
        if destination.is_symlink() or (destination.exists() and not destination.is_dir()):
            raise PrepareError(f'Output must be a normal directory: {engine["folder"]}')
        if any(_inside(path.resolve(), destination) for path in inputs):
            raise PrepareError('Keep original ROM and patch files outside the generated import folders')
        if destination.exists():
            for path in destination.rglob('*'):
                relative = path.relative_to(destination).as_posix()
                if path.is_symlink() or relative not in OWNED_PATHS:
                    raise PrepareError(f'Output contains unrelated files or links: {engine["folder"]}; choose another --output-root')
                if path.is_dir() != (relative == 'source'):
                    raise PrepareError(f'Output has an incompatible file layout: {engine["folder"]}')


def _write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def _publish(output_root: Path, engines: list[dict]) -> None:
    output_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='.tetris-prepare-', dir=output_root))
    backups, installed = [], []
    keep_recovery = False
    try:
        for engine in engines:
            staged = work / engine['folder']
            import_rom.generate(engine['rom'], staged, engine['title'], rights_acknowledged=True)
            _write_json(staged / 'patch_receipt.json', engine['patch_receipt'])
            _write_json(staged / 'preparation_receipt.json', engine['preparation_receipt'])
        try:
            for engine in engines:
                destination = output_root / engine['folder']
                if destination.exists():
                    backup = work / ('previous-' + engine['folder'])
                    backups.append((destination, backup))
                    os.replace(destination, backup)
            for engine in engines:
                destination = output_root / engine['folder']
                installed.append(destination)
                os.replace(work / engine['folder'], destination)
        except BaseException as failure:
            try:
                for destination in reversed(installed):
                    if destination.exists():
                        shutil.rmtree(destination)
                for destination, backup in reversed(backups):
                    if backup.exists():
                        os.replace(backup, destination)
            except BaseException as recovery:
                keep_recovery = True
                raise PrepareError(f'Output update failed; recovery files remain at {work}: {recovery}') from failure
            raise
    finally:
        if not keep_recovery:
            shutil.rmtree(work)


def prepare(rom, *, gym_patch=None, zero_patch=None, output_root=None,
            rights_acknowledged=False) -> dict:
    if not rights_acknowledged:
        raise PrepareError('Rights acknowledgment is required: ' + import_rom.RIGHTS_CONFIRMATION)
    if gym_patch is None and zero_patch is None:
        raise PrepareError('Supply --gym-patch, --zero-patch, or both')
    source_path = _local_path(rom, '.nes')
    patch_paths = {name: _local_path(value, '.bps' if name == 'gym' else '.ips')
                   for name, value in (('gym', gym_patch), ('zero', zero_patch)) if value is not None}
    destination_root = _local_path(output_root if output_root is not None else ROOT / '.local').resolve()
    source = _read(source_path)
    import_rom.inspect_rom(source)
    patches = {name: _read(path) for name, path in patch_paths.items()}
    engines = []
    for name, patch in patches.items():
        version = ENGINE_VERSIONS[name]
        if name == 'gym':
            result, patch_receipt = apply_bps.apply(source, patch, nes_mirroring_header=True)
        else:
            result, patch_receipt = apply_ips.apply(source, patch)
        metadata = import_rom.inspect_rom(result)
        actual = _identity(result)
        if actual['crc32'] != version['crc32'] or metadata['mapper'] != version['mapper']:
            raise PrepareError(f'{version["title"]} result is unsupported; expected {version["version"]} '
                               f'CRC32 {version["crc32"]}, got {actual["crc32"]}')
        receipt = {'format': 1, 'engine': name, 'title': version['title'], 'version': version['version'],
                   'rightsAcknowledged': True, 'source': {'name': source_path.name, **_identity(source)},
                   'patch': {'name': patch_paths[name].name, **_identity(patch)},
                   'target': {**metadata, **actual}, 'expected_crc32': version['crc32'],
                   'patch_receipt': 'patch_receipt.json', 'import_receipt': 'import_receipt.json'}
        engines.append({**version, 'rom': result, 'patch_receipt': patch_receipt,
                        'preparation_receipt': receipt})
    _check_outputs(destination_root, engines, [source_path, *patch_paths.values()])
    _publish(destination_root, engines)
    return {'format': 1, 'rightsAcknowledged': True,
            'engines': [engine['preparation_receipt'] for engine in engines]}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, help='Compatible local base .nes image')
    parser.add_argument('--gym-patch', help='Local TetrisGYM 6.0.0 BPS patch for solo play')
    parser.add_argument('--zero-patch', help='Local Tetris Zero MMC3_118_v2 IPS patch for two-player play')
    parser.add_argument('--output-root', help='Private root folder; defaults to this project\'s .local')
    parser.add_argument('--acknowledge-rights', action='store_true', help=import_rom.RIGHTS_CONFIRMATION)
    args = parser.parse_args(argv)
    if not args.acknowledge_rights:
        parser.error('--acknowledge-rights is required: ' + import_rom.RIGHTS_CONFIRMATION)
    try:
        receipt = prepare(args.rom, gym_patch=args.gym_patch, zero_patch=args.zero_patch,
                          output_root=args.output_root, rights_acknowledged=args.acknowledge_rights)
    except (PrepareError, apply_bps.PatchError, apply_ips.PatchError, import_rom.RomError, OSError) as exc:
        parser.exit(2, f'Tetris preparation failed: {exc}\n')
    print('Private Tetris images prepared: ' + ', '.join(engine['title'] for engine in receipt['engines']))
    print('Run python tools/build.py to build the device firmware.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
