#!/usr/bin/env python3
"""Prepare private Arduino configuration and pinned build dependencies."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CORE = "esp32:esp32@3.3.11"
LIBRARIES = ("M5Unified@0.2.21", "M5GFX@0.2.28", "NimBLE-Arduino@2.3.6")
FQBN = "esp32:esp32:esp32:FlashMode=dio,FlashFreq=40,FlashSize=4M,PSRAM=disabled,PartitionScheme=huge_app,UploadSpeed=115200,EraseFlash=none"
INDEX = "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"


def arduino_cli():
    path = shutil.which("arduino-cli")
    if not path:
        raise SystemExit("Install Arduino CLI first: https://arduino.github.io/arduino-cli/latest/installation/")
    return path


def ensure_config():
    config = ROOT / ".local" / "arduino-cli.yaml"
    if config.exists():
        return config
    cli = arduino_cli()
    data = subprocess.check_output([cli, "config", "get", "directories.data"], text=True).strip()
    if not data:
        raise SystemExit("Arduino CLI did not report its data directory")
    config.parent.mkdir(parents=True, exist_ok=True)
    settings = {
        "directories": {
            "data": data,
            "downloads": str(ROOT / ".local" / "downloads"),
            "user": str(ROOT / ".local" / "Arduino"),
        },
        "board_manager": {"additional_urls": [INDEX]},
    }
    config.write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
    return config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config-only", action="store_true")
    args = parser.parse_args()
    config = ensure_config()
    if args.config_only:
        print("Private Arduino configuration is ready.")
        return
    command = [arduino_cli(), "--config-file", str(config)]
    subprocess.run(command + ["core", "update-index"], check=True)
    subprocess.run(command + ["core", "install", CORE], check=True)
    subprocess.run(command + ["lib", "install", *LIBRARIES], check=True)
    print("Pinned build dependencies are ready. Import your local assets before building.")


if __name__ == "__main__":
    main()
