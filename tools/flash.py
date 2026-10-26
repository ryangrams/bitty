#!/usr/bin/env python3
"""Flash a locally built image to the explicitly selected serial port."""
import argparse
import subprocess
from setup import ROOT, FQBN, arduino_cli, ensure_config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port for your M5 device")
    args = parser.parse_args()
    build = ROOT / ".local" / "build"
    if not (build / "flappy.ino.bin").is_file():
        raise SystemExit("Build the private firmware first: python3 tools/build.py")
    subprocess.run([arduino_cli(), "--config-file", str(ensure_config()), "upload", "--fqbn", FQBN,
                    "--port", args.port, "--input-dir", str(build),
                    str(ROOT / ".local" / "sketch" / "flappy")], check=True)
    print("Flash complete. Close serial monitors before playing.")


if __name__ == "__main__":
    main()
