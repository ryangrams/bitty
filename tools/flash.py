#!/usr/bin/env python3
"""Flash a locally built image to the explicitly selected serial port."""
import argparse
import hashlib
import json
import subprocess
from setup import BOARD_FQBN, build_directory, sketch_directory, arduino_cli, ensure_config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port for your M5 device")
    parser.add_argument("--board", choices=tuple(BOARD_FQBN), default="stickc", help="Must match the board used for the build")
    args = parser.parse_args()
    build = build_directory(args.board)
    if not (build / "flappy.ino.bin").is_file():
        raise SystemExit("Build the private firmware first: python3 tools/build.py")
    try:
        receipt = json.loads((build / "build-target.json").read_text())
    except (OSError, ValueError) as error:
        raise SystemExit("Rebuild with tools/build.py to create the hardware target receipt") from error
    if receipt.get("board") != args.board or receipt.get("fqbn") != BOARD_FQBN[args.board] or \
            receipt.get("image_sha256") != hashlib.sha256((build / "flappy.ino.bin").read_bytes()).hexdigest():
        raise SystemExit("The firmware does not match its hardware target receipt. Rebuild before flashing")
    subprocess.run([arduino_cli(), "--config-file", str(ensure_config()), "upload", "--fqbn", BOARD_FQBN[args.board],
                    "--port", args.port, "--input-dir", str(build),
                    str(sketch_directory(args.board))], check=True)
    print("Flash complete. Close serial monitors before playing.")


if __name__ == "__main__":
    main()
