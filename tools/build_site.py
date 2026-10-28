#!/usr/bin/env python3
"""Package the USB installer with a verified public starter build."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def package(build, output, version, dependencies, preview=False):
    origin = json.loads((build / 'public-origin.json').read_text())
    image = (build / 'flappy.ino.bin').read_bytes()
    if origin.get('public') is not True or origin.get('library') is not True or origin.get('display') != 'stickc-plus':
        raise ValueError('A public USB-library build for StickC Plus is required')
    if hashlib.sha256(image).hexdigest() != origin.get('image_sha256') or not 16 <= len(image) <= 0x200000:
        raise ValueError('The starter image changed or exceeds its 2 MiB partition')
    sketch = ROOT / '.local/sketch/flappy'
    for name, checksum in origin['sketch_sha256'].items():
        if Path(name).name != name or hashlib.sha256((sketch / name).read_bytes()).hexdigest() != checksum:
            raise ValueError('The public build inputs changed')
    partitions = (build / 'flappy.ino.partitions.bin').read_bytes()
    entries = []
    for pos in range(0, len(partitions), 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from('<HBBII16sI', partitions, pos)
        if magic == 0x50aa:
            entries.append((label.rstrip(b'\0').decode(), offset, size))
    if ('app0',0x10000,0x200000) not in entries or ('library',0x210000,0x1e0000) not in entries or ('nvs',0x9000,0x5000) not in entries:
        raise ValueError('The compiled partition table does not match the library layout')
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    for path in (ROOT / 'web').iterdir():
        if path.is_file() and path.suffix in ('.html','.css','.mjs'):
            shutil.copy2(path, output / path.name)
    destination = output / 'firmware'
    destination.mkdir()
    parts, checksums = [], {}
    for source, target, offset in [('flappy.ino.bootloader.bin','bootloader.bin',0x1000),
                                   ('flappy.ino.partitions.bin','partitions.bin',0x8000),
                                   ('boot_app0.bin','boot_app0.bin',0xe000),
                                   ('flappy.ino.bin','bitty.bin',0x10000)]:
        data = (build / source).read_bytes()
        # Arduino has already patched both image headers to DIO/80 MHz/4 MiB.
        # Separate parts preserve the NVS and cartridge regions during updates.
        if source in ('flappy.ino.bootloader.bin','flappy.ino.bin') and data[:4] != bytes([0xe9,data[1],2,0x2f]):
            raise ValueError('ESP32 image header must declare DIO/80 MHz/4 MiB')
        (destination / target).write_bytes(data)
        checksums[target] = hashlib.sha256(data).hexdigest()
        parts.append({'path':target,'offset':offset})
    manifest = {'name':'Bitty','version':version,'new_install_prompt_erase':True,
                'new_install_improv_wait_time':0,'builds':[{'chipFamily':'ESP32','improv':False,'parts':parts}]}
    (destination / 'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (destination / 'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name,digest in checksums.items()))
    (destination / 'provenance.json').write_text(json.dumps(origin,indent=2)+'\n')
    if preview:
        shutil.rmtree(destination)
    (output / 'release.json').write_text(json.dumps({'available': not preview, 'version': version})+'\n')
    esbuild = dependencies / 'node_modules/esbuild/bin/esbuild'
    source = dependencies / 'node_modules/esp-web-tools/dist/install-button.js'
    subprocess.run([str(esbuild),str(source),'--bundle','--format=esm','--minify',
                    '--outfile='+str(output/'installer.js'),'--legal-comments=linked'],check=True)
    licenses = output / 'licenses'
    licenses.mkdir()
    for metadata in (dependencies / 'node_modules').rglob('package.json'):
        package_root = metadata.parent
        info = json.loads(metadata.read_text())
        name = info.get('name','').replace('@','').replace('/','-')
        if not name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-' for c in name):
            continue
        for path in package_root.iterdir():
            if path.is_file() and path.name.lower() in ('license','license.md','license.txt','copying','copying.md','copying.txt','notice','notice.md','notice.txt'):
                shutil.copy2(path, licenses / (name + '-' + path.name))
    (output / '.nojekyll').write_text('')
    print('Public USB installer packaged:', output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version',default='0.1.0-beta')
    parser.add_argument('--reuse-public-build',action='store_true')
    parser.add_argument('--preview-only',action='store_true',help='Publish the interface without downloadable firmware')
    args = parser.parse_args()
    if not args.reuse_public_build:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--public','--library','--display','stickc-plus'],check=True)
    dependencies = ROOT / '.local/web-deps'
    dependencies.mkdir(parents=True,exist_ok=True)
    for name in ('package.json','package-lock.json'):
        shutil.copy2(ROOT/'web'/name, dependencies/name)
    subprocess.run(['npm','ci','--prefix',str(dependencies),'--no-audit','--no-fund'],check=True)
    package(ROOT/'.local/build',ROOT/'.local/site',args.version,dependencies,args.preview_only)


if __name__ == '__main__':
    main()
