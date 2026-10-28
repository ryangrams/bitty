#!/usr/bin/env python3
"""Compile a private firmware image using locally imported assets."""
import argparse
from datetime import datetime
from pathlib import Path
import json
import hashlib
import shutil
import subprocess
from setup import ROOT, FQBN, arduino_cli, ensure_config

HEADERS = ("generated_assets.h", "generated_sounds.h", "generated_collision.h")
DISPLAY_PROFILES = {"auto": 0, "stickc": 1, "stickc-plus": 2}
MAX_DISPLAY_PROFILE_BYTES = 128


def resolve_display_profile(display=None):
    profile = ROOT / ".local" / "display-profile.json"
    if display is not None:
        if not isinstance(display, str) or display not in DISPLAY_PROFILES:
            raise SystemExit("Display profile must be auto, stickc, or stickc-plus")
        profile.parent.mkdir(parents=True, exist_ok=True)
        profile.write_text(json.dumps(display) + "\n", encoding="utf-8")
        return display
    if not profile.exists():
        return "auto"
    try:
        with profile.open("rb") as source:
            data = source.read(MAX_DISPLAY_PROFILE_BYTES + 1)
        if len(data) > MAX_DISPLAY_PROFILE_BYTES:
            raise ValueError("file exceeds the size limit")
        saved = json.loads(data.decode("utf-8"))
        if not isinstance(saved, str) or saved not in DISPLAY_PROFILES:
            raise ValueError("expected auto, stickc, or stickc-plus")
    except (OSError, ValueError, UnicodeError) as exc:
        raise SystemExit("Invalid private display profile. Choose --display auto, --display stickc, "
                         "or --display stickc-plus to replace it: " + str(exc)) from exc
    return saved


def prepare_sketch(assets, skins=None, rom=None, display=None, tetris=None, zero=None, geometry=None, jetpack=None, public=False, library=False):
    (ROOT / ".local" / "build" / "public-origin.json").unlink(missing_ok=True)
    if public:
        if any(value is not None and value is not False for value in (skins, rom, tetris, zero, geometry, jetpack)):
            raise SystemExit("Public builds accept only project-created inputs")
        from starter_assets import generate
        assets = generate(ROOT / ".local" / "starter-assets")
        jetpack = False
    missing = [name for name in HEADERS if not (assets / name).is_file()]
    if missing:
        raise SystemExit("Import your local assets first. Missing: " + ", ".join(missing) +
                         "\nSee assets/README.md for the local atlas or APK import command.")
    if skins is not None and not (skins / "generated_skins.h").is_file():
        raise SystemExit("Generate your custom skins first. Missing: generated_skins.h in " + str(skins))
    if rom is None and not public and not library:
        default_rom = ROOT / ".local" / "nes-rom"
        if (default_rom / "generated_rom.h").is_file():
            rom = default_rom
    if rom is not None and not (rom / "generated_rom.h").is_file():
        raise SystemExit("Import your local ROM first. Missing: generated_rom.h in " + str(rom))
    if tetris is None and not public and not library:
        default_tetris = ROOT / ".local" / "tetris-rom"
        if (default_tetris / "generated_rom.h").is_file():
            tetris = default_tetris
    if tetris is not None and not (tetris / "generated_rom.h").is_file():
        raise SystemExit("Import your patched Tetris ROM first. Missing: generated_rom.h")
    if zero is None and not public and not library:
        default_zero = ROOT / ".local" / "tetris-zero-rom"
        if (default_zero / "generated_rom.h").is_file():
            zero = default_zero
    if zero is not None and not (zero / "generated_rom.h").is_file():
        raise SystemExit("Import your Tetris Zero ROM first. Missing: generated_rom.h")
    if geometry is None and not public and not library:
        default_geometry = ROOT / ".local" / "geometry-rom"
        if (default_geometry / "generated_rom.h").is_file():
            geometry = default_geometry
    if geometry is not None and not (geometry / "generated_rom.h").is_file():
        raise SystemExit("Import your Geometry Dash ROM first. Missing: generated_rom.h in " + str(geometry))
    if jetpack is None:
        default_jetpack = ROOT / ".local" / "jetpack" / "imported"
        if (default_jetpack / "jetpack_imported.h").is_file():
            jetpack = default_jetpack
    jetpack_header = None
    if jetpack is not None and jetpack is not False:
        path = jetpack / "jetpack_imported.h"
        if not path.is_file() or path.stat().st_size > 1024 * 1024:
            raise SystemExit("Import your Jetpack reference pack first. Missing or oversized: jetpack_imported.h")
        try:
            jetpack_header = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            raise SystemExit("Cannot read private Jetpack import header: " + str(exc)) from exc
    selected_display = resolve_display_profile(display)
    target = ROOT / ".local" / "sketch" / "flappy"
    target.mkdir(parents=True, exist_ok=True)
    for existing in target.iterdir():
        if existing.is_file():
            existing.unlink()
    for path in (ROOT / "firmware" / "flappy").iterdir():
        if path.suffix in (".h", ".ino"):
            shutil.copy2(path, target / path.name)
    if library:
        shutil.copy2(ROOT / "firmware" / "flappy" / "partitions.csv", target / "partitions.csv")
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
    from import_rom import rom_header
    tetris_header = (tetris / "generated_rom.h").read_text(encoding="utf-8") if tetris else rom_header(None, "Tetris")
    (target / "generated_tetris_rom.h").write_text(tetris_header.replace("namespace FlappyRom", "namespace TetrisRom"), encoding="utf-8")
    zero_header = (zero / "generated_rom.h").read_text(encoding="utf-8") if zero else rom_header(None, "Tetris Zero")
    (target / "generated_zero_rom.h").write_text(zero_header.replace("namespace FlappyRom", "namespace TetrisZeroRom"), encoding="utf-8")
    geometry_header = (geometry / "generated_rom.h").read_text(encoding="utf-8") if geometry else rom_header(None, "Geometry Dash")
    (target / "generated_geometry_rom.h").write_text(geometry_header.replace("namespace FlappyRom", "namespace GeometryRom"), encoding="utf-8")
    if jetpack_header is not None:
        (target / "jetpack_imported.h").write_text(jetpack_header, encoding="utf-8")
    (target / "build_display.h").write_text(
        "#pragma once\n#define M5FLAP_DISPLAY_PROFILE " + str(DISPLAY_PROFILES[selected_display]) + "\n" +
        ("#define M5FLAP_USB_LIBRARY 1\n" if library else "") +
        ("#define M5FLAP_JETPACK_IMPORTED 1\n" if jetpack_header is not None else ""),
        encoding="utf-8")
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
    parser.add_argument("--tetris-dir", type=Path, help="Private patched Tetris import folder (default: .local/tetris-rom when present)")
    parser.add_argument("--zero-dir", type=Path, help="Private Tetris Zero import folder (default: .local/tetris-zero-rom when present)")
    parser.add_argument("--geometry-dir", type=Path, help="Private Geometry Dash import folder (default: .local/geometry-rom when present)")
    jetpack_group = parser.add_mutually_exclusive_group()
    jetpack_group.add_argument("--jetpack-dir", type=Path, help="Private Jetpack import folder (default: .local/jetpack/imported when present)")
    jetpack_group.add_argument("--no-jetpack-import", action="store_true", help="Use project-created Jetpack coin formations and artwork")
    parser.add_argument("--display", choices=tuple(DISPLAY_PROFILES),
                        help="LCD profile, saved privately for later builds (default: saved profile or auto)")
    parser.add_argument("--check-only", action="store_true", help="Prepare the private sketch without compiling")
    parser.add_argument("--public", action="store_true", help="Build exclusively from project-created assets, with no local ROM or APK discovery")
    parser.add_argument("--library", action="store_true", help="Use the USB cartridge partition instead of embedded ROMs")
    args = parser.parse_args()
    sketch = prepare_sketch(args.assets_dir.resolve(), args.skins_dir.resolve() if args.skins_dir else None,
                            args.rom_dir.resolve() if args.rom_dir else None, args.display,
                            args.tetris_dir.resolve() if args.tetris_dir else None,
                            args.zero_dir.resolve() if args.zero_dir else None,
                            args.geometry_dir.resolve() if args.geometry_dir else None,
                            False if args.no_jetpack_import else args.jetpack_dir.resolve() if args.jetpack_dir else None,
                            public=args.public, library=args.library)
    if args.check_only:
        print("Private sketch prepared. Generated assets remain outside the public source tree.")
        return
    from setup_nes import prepare
    library = prepare()
    build = ROOT / ".local" / "build"
    subprocess.run([arduino_cli(), "--config-file", str(ensure_config()), "compile", "--fqbn", FQBN,
                    "--build-property", "compiler.optimization_flags=-O2 -mno-fix-esp32-psram-cache-issue",
                    "--build-path", str(build), "--library", str(library), str(sketch)], check=True)
    if args.public:
        image = build / "flappy.ino.bin"
        origin = {"public": True, "library": args.library, "display": resolve_display_profile(),
                  "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                  "sketch_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in sorted(sketch.iterdir()) if p.is_file()}}
        (build / "public-origin.json").write_text(json.dumps(origin, indent=2) + "\n")
    print("Firmware is in .local/build. Keep asset-bearing firmware private.")


if __name__ == "__main__":
    main()
