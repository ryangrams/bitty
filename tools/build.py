#!/usr/bin/env python3
"""Compile a private firmware image using locally imported assets."""
import argparse
from datetime import datetime
from pathlib import Path
import shutil
import subprocess
from setup import ROOT, FQBN, arduino_cli, ensure_config

HEADERS = ("generated_assets.h", "generated_sounds.h", "generated_collision.h")


def prepare_sketch(assets, skins=None, rom=None):
    missing = [name for name in HEADERS if not (assets / name).is_file()]
    if missing:
        raise SystemExit("Import your local assets first. Missing: " + ", ".join(missing) +
                         "\nSee assets/README.md for the local atlas or APK import command.")
    if skins is not None and not (skins / "generated_skins.h").is_file():
        raise SystemExit("Generate your custom skins first. Missing: generated_skins.h in " + str(skins))
    if rom is None:
        default_rom = ROOT / ".local" / "nes-rom"
        if (default_rom / "generated_rom.h").is_file():
            rom = default_rom
    if rom is not None and not (rom / "generated_rom.h").is_file():
        raise SystemExit("Import your local ROM first. Missing: generated_rom.h in " + str(rom))
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
    if skins is None:
        from generate_skins import skin_header
        (target / "generated_skins.h").write_text(skin_header([])[0], encoding="utf-8")
    else:
        shutil.copy2(skins / "generated_skins.h", target / "generated_skins.h")
    if rom is None:
        from import_rom import rom_header
        (target / "generated_rom.h").write_text(rom_header(None), encoding="utf-8")
    else:
        shutil.copy2(rom / "generated_rom.h", target / "generated_rom.h")
    now = datetime.now()
    clock = ["// Generated from the build host's local clock.", "#pragma once", "namespace FlappyBuild {"]
    clock.extend(f"static constexpr int {name} = {getattr(now, name)};"
                 for name in ("year", "month", "day", "hour", "minute", "second"))
    clock.append("}  // namespace FlappyBuild")
    (target / "build_clock.h").write_text("\n".join(clock) + "\n", encoding="utf-8")
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-dir", type=Path, default=ROOT / ".local" / "assets")
    parser.add_argument("--skins-dir", type=Path, help="Private folder containing generated_skins.h")
    parser.add_argument("--rom-dir", type=Path, help="Private folder containing generated_rom.h (default: .local/nes-rom when present)")
    parser.add_argument("--check-only", action="store_true", help="Prepare the private sketch without compiling")
    args = parser.parse_args()
    sketch = prepare_sketch(args.assets_dir.resolve(), args.skins_dir.resolve() if args.skins_dir else None,
                            args.rom_dir.resolve() if args.rom_dir else None)
    if args.check_only:
        print("Private sketch prepared. Generated assets remain outside the public source tree.")
        return
    from setup_nes import prepare
    library = prepare()
    build = ROOT / ".local" / "build"
    subprocess.run([arduino_cli(), "--config-file", str(ensure_config()), "compile", "--fqbn", FQBN,
                    "--build-path", str(build), "--library", str(library), str(sketch)], check=True)
    print("Firmware is in .local/build. Keep asset-bearing firmware private.")


if __name__ == "__main__":
    main()
