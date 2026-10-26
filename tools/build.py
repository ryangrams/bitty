#!/usr/bin/env python3
"""Compile a private firmware image using locally imported assets."""
import argparse
from pathlib import Path
import shutil
import subprocess
from setup import ROOT, FQBN, arduino_cli, ensure_config

HEADERS = ("generated_assets.h", "generated_sounds.h", "generated_collision.h")


def prepare_sketch(assets):
    missing = [name for name in HEADERS if not (assets / name).is_file()]
    if missing:
        raise SystemExit("Import your local assets first. Missing: " + ", ".join(missing) +
                         "\nSee assets/README.md for the local atlas or APK import command.")
    target = ROOT / ".local" / "sketch" / "flappy"
    target.mkdir(parents=True, exist_ok=True)
    for existing in target.iterdir():
        if existing.is_file():
            existing.unlink()
    for path in (ROOT / "firmware" / "flappy").iterdir():
        if path.suffix in (".h", ".ino"):
            shutil.copy2(path, target / path.name)
    for name in HEADERS:
        shutil.copy2(assets / name, target / name)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-dir", type=Path, default=ROOT / ".local" / "assets")
    parser.add_argument("--check-only", action="store_true", help="Prepare the private sketch without compiling")
    args = parser.parse_args()
    sketch = prepare_sketch(args.assets_dir.resolve())
    if args.check_only:
        print("Private sketch prepared. Generated assets remain outside the public source tree.")
        return
    build = ROOT / ".local" / "build"
    subprocess.run([arduino_cli(), "--config-file", str(ensure_config()), "compile", "--fqbn", FQBN,
                    "--build-path", str(build), str(sketch)], check=True)
    print("Firmware is in .local/build. Keep asset-bearing firmware private.")


if __name__ == "__main__":
    main()
