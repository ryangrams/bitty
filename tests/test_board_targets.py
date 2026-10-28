"""Check board isolation and hardware-target verification without a device."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build
import flash
import setup


class BoardTargetTests(unittest.TestCase):
    def test_sticks3_does_not_reuse_or_change_a_saved_stickc_display(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            assets = root / 'assets'
            assets.mkdir()
            for name in build.HEADERS:
                (assets / name).write_text('// fixture\n')
            firmware = root / 'firmware/flappy'
            firmware.mkdir(parents=True)
            (firmware / 'flappy.ino').write_text('// fixture\n')
            partitions = (Path(__file__).resolve().parents[1] / 'firmware/flappy/partitions_sticks3.csv').read_text()
            (firmware / 'partitions_sticks3.csv').write_text(partitions)
            local = root / '.local'
            local.mkdir()
            saved = local / 'display-profile.json'
            saved.write_text('"stickc-plus"\n')
            existing = local / 'sketch/flappy'
            existing.mkdir(parents=True)
            (existing / 'keep.txt').write_text('old board sketch')
            with mock.patch.object(build, 'ROOT', root):
                target = build.prepare_sketch(assets, board='sticks3', library=True, jetpack=False)
                self.assertEqual(target, local / 'sketch-sticks3/flappy')
                self.assertIn('M5FLAP_DISPLAY_PROFILE 0', (target / 'build_display.h').read_text())
                self.assertIn('M5FLAP_BOARD_STICKS3 1', (target / 'build_display.h').read_text())
                self.assertEqual((target / 'partitions.csv').read_text(), partitions)
                with self.assertRaisesRegex(SystemExit, 'own automatic LCD driver'):
                    build.prepare_sketch(assets, board='sticks3', display='stickc-plus')
            self.assertEqual(saved.read_text(), '"stickc-plus"\n')
            self.assertEqual((existing / 'keep.txt').read_text(), 'old board sketch')

    def test_s3_uses_octal_psram_native_usb_and_eight_megabyte_flash(self):
        fqbn = setup.BOARD_FQBN['sticks3']
        for setting in ('esp32:esp32:esp32s3:', 'FlashSize=8M', 'PSRAM=opi', 'USBMode=hwcdc', 'CDCOnBoot=cdc'):
            self.assertIn(setting, fqbn)
        self.assertNotEqual(fqbn, setup.BOARD_FQBN['stickc'])
        text = (Path(__file__).resolve().parents[1] / 'firmware/flappy/partitions_sticks3.csv').read_text()
        entries = [line.split(',') for line in text.splitlines() if line and not line.startswith('#')]
        end = 0
        for entry in entries:
            offset, size = int(entry[3], 0), int(entry[4], 0)
            self.assertGreaterEqual(offset, end)
            end = offset + size
        self.assertEqual(end, 8 * 1024 * 1024)

    def test_upload_requires_the_matching_board_and_unchanged_image(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / '.local/build-sticks3'
            target.mkdir(parents=True)
            image = target / 'flappy.ino.bin'
            image.write_bytes(b'synthetic firmware')
            receipt = dict(board='sticks3', fqbn=setup.BOARD_FQBN['sticks3'],
                           image_sha256=hashlib.sha256(image.read_bytes()).hexdigest())
            metadata = target / 'build-target.json'
            with mock.patch.object(setup, 'ROOT', root), \
                    mock.patch.object(flash, 'arduino_cli', return_value='arduino-cli'), \
                    mock.patch.object(flash, 'ensure_config', return_value=root / 'config'), \
                    mock.patch.object(sys, 'argv', ['flash.py', '--board', 'sticks3', '--port', 'fixture']), \
                    mock.patch.object(flash.subprocess, 'run') as execute:
                with self.assertRaisesRegex(SystemExit, 'target receipt'):
                    flash.main()
                metadata.write_text(json.dumps(dict(receipt, board='stickc')))
                with self.assertRaisesRegex(SystemExit, 'does not match'):
                    flash.main()
                metadata.write_text(json.dumps(receipt))
                image.write_bytes(b'changed')
                with self.assertRaisesRegex(SystemExit, 'does not match'):
                    flash.main()
                execute.assert_not_called()
                image.write_bytes(b'synthetic firmware')
                flash.main()
                command = execute.call_args.args[0]
                self.assertEqual(command[command.index('--fqbn') + 1], receipt['fqbn'])
                self.assertEqual(command[command.index('--input-dir') + 1], str(target))


if __name__ == '__main__':
    unittest.main()
