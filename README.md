# M5 Flap

A one-button flight game engine for the ESP32-based M5StickC family. It boots into the game, keeps a best score, and supports compatible artwork and optional sound supplied by the person building it.

This repository contains MIT-licensed code, tools, and synthetic test fixtures. Supply your own compatible assets locally. Original Flappy Bird artwork, sounds, logos, screenshots, sprite silhouettes, and asset-bearing firmware are excluded from the public source package. This is an independent project with no affiliation with the original game's creators or M5Stack.

## Build your copy

Use Python 3.11 or newer, [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/), and a C++ compiler for host tests. Install FFmpeg if you want to convert supplied audio. An atlas-only import produces a silent game.

1. Create a Python environment and install the importer dependency:

   ```sh
   python3 -m venv .local/venv
   . .local/venv/bin/activate
   python -m pip install -r requirements.txt
   ```

   On Windows, activate `.local\venv\Scripts\activate`.

2. Install the pinned Arduino dependencies:

   ```sh
   python tools/setup.py
   ```

3. Import local files you are entitled to use:

   ```sh
   python tools/generate_assets.py --atlas /path/to/atlas.png --manifest /path/to/atlas.txt
   ```

   A compatible local APK is also accepted:

   ```sh
   python tools/generate_assets.py --apk /path/to/your-copy.apk
   ```

   The importer makes no network requests. See [asset input details](assets/README.md) for format requirements, optional audio, and original-art alternatives.

4. Build, then select your device's serial port explicitly:

   ```sh
   python tools/build.py
   python tools/flash.py --port /dev/cu.YOUR_DEVICE
   ```

   Windows serial ports use names such as `COM3`. The build never opens a serial port. Flashing is a separate command. Close serial monitors before playing.

All imported inputs, generated headers, staged sketches, and firmware stay under ignored `.local/`. Dependencies use the system's existing Arduino data directory and a private sketchbook. The ESP32 core is pinned to 3.3.11, M5Unified to 0.2.21, and M5GFX to 0.2.28.

## Play and screen fit

Press the front M5 button or the side game button to flap. After a crash, wait for replay and press once to return to the ready screen. The next press starts a round. The best score survives a restart.

The renderer uses a 144 by 256 pixel canvas. A 135 by 240 display crops 4 pixels from the left, 5 from the right, and 8 from the top and bottom, preserving the underlying sprite grid. An 80 by 160 display fits the whole scene at 80 by 142 with nearest-neighbor sampling. A full original phone frame cannot fit on these screens with every source pixel visible. The importer verifies exact duplicated 2 by 2 blocks before reducing a compatible atlas.

## Tests and source distribution

```sh
python tools/test.py
python tools/check_distribution.py --git
python tools/package_source.py --git
```

When using an extracted source ZIP, omit `--git`. A Git checkout is audited automatically, including its reachable history.

Tests create geometric assets in temporary directories. They exercise asset import, collision-mask generation, gameplay, rendering, and publication checks without original game media. CI runs those checks and packages source. It does not publish firmware, screenshots, or generated assets.

The exporter uses an explicit [file allowlist](PUBLIC_FILES.txt), checks tracked files and reachable Git history, and writes `.local/releases/m5-flap-source.zip`. It rejects known asset paths, generated headers, binaries, symlinks, original download URLs, and the previous embedded silhouette data. These checks support review; they do not determine ownership or legal clearance.

Use the source exporter for public release attachments. Firmware built with imported assets needs separate distribution permission. Avoid adding private files with `git add -f`; removing a file in a later commit leaves earlier copies in Git history.

## Rights and attribution

The code is under [MIT](LICENSE), with [upstream notices](THIRD_PARTY_NOTICES.md). Imported assets retain their own rights. A game's availability on Archive.org, an uploader's credit, or possession of an APK does not by itself grant permission for the intended use. The [Internet Archive's rights guidance](https://help.archive.org/help/rights/) makes that distinction explicit.

Read the [rights boundary](docs/RIGHTS.md) before distributing a build or changing the public package. Separating assets from source reduces what this project distributes. It cannot guarantee legal clearance for every imported asset, finished game presentation, or jurisdiction.

## Local storage

The original development media, generated headers, firmware images, device backups, and machine-specific notes were moved into ignored `.local/` while preparing this source distribution. Nothing was uploaded. Installed local toolchains remain ignored under `toolchain/`. Temporary build caches and test executables can be deleted and recreated with the commands above. Preserve any private input files you cannot recover before deleting `.local/`.
