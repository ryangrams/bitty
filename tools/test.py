#!/usr/bin/env python3
"""Run source-only tests without downloading or importing game artwork."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-sanitize", action="store_true")
    args = parser.parse_args()
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py", "-v"],
                   cwd=ROOT, check=True)
    compiler = os.environ.get("CXX") or shutil.which("c++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("A C++11 compiler is required for the gameplay checks")
    target = ROOT / ".local" / "tests"
    target.mkdir(parents=True, exist_ok=True)
    executable = target / ("game_core_test.exe" if os.name == "nt" else "game_core_test")
    flags = ["-std=c++11", "-Wall", "-Wextra", "-Werror"]
    if not args.no_sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run([compiler] + flags + [str(ROOT / "tests" / "game_core_test.cpp"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    sys.path.insert(0, str(ROOT / "tests"))
    from test_asset_import import make_fixture
    from generate_assets import generate
    with tempfile.TemporaryDirectory(dir=target) as temporary:
        fixture = Path(temporary)
        atlas, manifest = make_fixture(fixture)
        assets = fixture / "generated"
        generate(atlas.read_bytes(), manifest.read_bytes(), {}, assets)
        preview = fixture / ("render_preview.exe" if os.name == "nt" else "render_preview")
        subprocess.run([compiler] + flags + ["-I", str(assets), str(ROOT / "tests" / "render_preview.cpp"),
                                            "-o", str(preview)], check=True)
        for width, height in ((80, 160), (135, 240), (144, 256)):
            for state in (0, 1, 2):
                subprocess.run([str(preview), str(width), str(height), str(state)],
                               stdout=subprocess.DEVNULL, check=True)
    print("Source-only checks passed. No proprietary assets were used.")


if __name__ == "__main__":
    main()
