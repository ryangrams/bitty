"""Keep private Jetpack inputs out of builds that use the authored fallback."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build


class JetpackBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        firmware = self.root / "firmware" / "flappy"
        firmware.mkdir(parents=True)
        (firmware / "flappy.ino").write_text("// Synthetic sketch.\n")
        self.assets = self.root / "assets"
        self.assets.mkdir()
        for name in build.HEADERS:
            (self.assets / name).write_text("// Synthetic fixture.\n")
        self.pack = self.root / ".local" / "jetpack" / "imported"
        self.pack.mkdir(parents=True)
        self.header = "#pragma once\nnamespace flappy { namespace JetpackImported {} }\n"

    def test_private_default_override_and_explicit_fallback(self):
        with mock.patch.object(build, "ROOT", self.root):
            stage = build.prepare_sketch(self.assets)
            self.assertNotIn("M5FLAP_JETPACK_IMPORTED", (stage / "build_display.h").read_text())
            (self.pack / "jetpack_imported.h").write_text(self.header)
            stage = build.prepare_sketch(self.assets)
            self.assertEqual((stage / "jetpack_imported.h").read_text(), self.header)
            self.assertIn("M5FLAP_JETPACK_IMPORTED 1", (stage / "build_display.h").read_text())
            override = self.root / "custom"
            override.mkdir()
            (override / "jetpack_imported.h").write_text(self.header + "// Override.\n")
            stage = build.prepare_sketch(self.assets, jetpack=override)
            self.assertTrue((stage / "jetpack_imported.h").read_text().endswith("// Override.\n"))
            stage = build.prepare_sketch(self.assets, jetpack=False)
            self.assertFalse((stage / "jetpack_imported.h").exists())
            self.assertNotIn("M5FLAP_JETPACK_IMPORTED", (stage / "build_display.h").read_text())
            self.assertEqual((self.pack / "jetpack_imported.h").read_text(), self.header)

    def test_bad_explicit_pack_preserves_previous_stage(self):
        with mock.patch.object(build, "ROOT", self.root):
            stage = build.prepare_sketch(self.assets)
            before = {p.name: p.read_bytes() for p in stage.iterdir()}
            for data in (None, b"\xff", b"x" * (1024 * 1024 + 1)):
                path = self.pack / "jetpack_imported.h"
                if data is not None:
                    path.write_bytes(data)
                with self.assertRaises(SystemExit):
                    build.prepare_sketch(self.assets, jetpack=self.pack)
                self.assertEqual(before, {p.name: p.read_bytes() for p in stage.iterdir()})

    def test_cli_forwards_pack_and_fallback(self):
        for flags, expected in ((["--jetpack-dir", str(self.pack)], self.pack.resolve()),
                                (["--no-jetpack-import"], False)):
            with mock.patch.object(build, "prepare_sketch") as prepare, \
                    mock.patch.object(sys, "argv", ["build.py", "--check-only"] + flags), \
                    contextlib.redirect_stdout(io.StringIO()):
                build.main()
            self.assertEqual(prepare.call_args.args[7], expected)


if __name__ == "__main__":
    unittest.main()
