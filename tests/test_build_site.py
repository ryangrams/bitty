"""Public installer packaging rejects stale or private build inputs."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_site


class InstallerPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / '.local/build'
        self.build.mkdir(parents=True)
        self.sketch = self.root / '.local/sketch/flappy'
        self.sketch.mkdir(parents=True)
        (self.sketch / 'build_display.h').write_text('#define SYNTHETIC 1\n')
        web = self.root / 'web'
        web.mkdir()
        (web / 'index.html').write_text('<title>Synthetic installer</title>')
        self.dependencies = self.root / 'deps'
        (self.dependencies / 'node_modules').mkdir(parents=True)
        self.output = self.root / 'site'
        self.image = bytes([0xe9, 1, 2, 0x2f]) + bytes(32)
        for name in ['flappy.ino.bin', 'flappy.ino.bootloader.bin', 'boot_app0.bin']:
            (self.build / name).write_bytes(self.image)
        parts = []
        for name, offset, size in [('nvs', 0x9000, 0x5000), ('app0', 0x10000, 0x200000),
                                   ('library', 0x210000, 0x1e0000)]:
            parts.append(struct.pack('<HBBII16sI', 0x50aa, 1, 0, offset, size, name.encode(), 0))
        (self.build / 'flappy.ino.partitions.bin').write_bytes(b''.join(parts))
        self.origin = dict(public=True, library=True, display='stickc-plus',
                           image_sha256=hashlib.sha256(self.image).hexdigest(),
                           sketch_sha256={'build_display.h': hashlib.sha256((self.sketch / 'build_display.h').read_bytes()).hexdigest()})
        self.save_origin()
        patcher = mock.patch.object(build_site, 'ROOT', self.root)
        patcher.start()
        self.addCleanup(patcher.stop)

    def save_origin(self):
        (self.build / 'public-origin.json').write_text(json.dumps(self.origin))

    def package(self, preview=False):
        with mock.patch.object(build_site.subprocess, 'run'):
            build_site.package(self.build, self.output, 'test', self.dependencies, preview)

    def test_preview_contains_no_downloadable_firmware(self):
        self.package(preview=True)
        self.assertFalse((self.output / 'firmware').exists())
        self.assertFalse(json.loads((self.output / 'release.json').read_text())['available'])

    def test_separate_flash_parts_preserve_library_and_nvs(self):
        self.package()
        manifest = json.loads((self.output / 'firmware/manifest.json').read_text())
        parts = manifest['builds'][0]['parts']
        self.assertEqual([p['offset'] for p in parts], [0x1000, 0x8000, 0xe000, 0x10000])
        self.assertTrue(manifest['new_install_prompt_erase'])
        self.assertNotIn('merged.bin', [p['path'] for p in parts])
        self.assertEqual((self.output / 'firmware/bitty.bin').read_bytes(), self.image)

    def test_private_wrong_board_and_non_library_builds_rejected(self):
        for key, value in [('public', False), ('library', False), ('display', 'stickc')]:
            with self.subTest(key=key):
                original = self.origin[key]
                self.origin[key] = value
                self.save_origin()
                with self.assertRaises(ValueError):
                    self.package()
                self.origin[key] = original
        self.save_origin()

    def test_changed_image_staged_source_and_partition_are_rejected(self):
        for path in [self.build / 'flappy.ino.bin', self.sketch / 'build_display.h',
                     self.build / 'flappy.ino.partitions.bin']:
            with self.subTest(path=path.name):
                original = path.read_bytes()
                path.write_bytes(bytes(len(original)))
                with self.assertRaises(ValueError):
                    self.package()
                path.write_bytes(original)


if __name__ == '__main__':
    unittest.main()
