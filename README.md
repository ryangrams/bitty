# M5 Flap

A small flight game for ESP32-based M5StickC devices, with a front action button, a side-button menu, a flying clock, and a two-device Bluetooth race. Original mode uses compatible artwork and optional sound that you supply locally.

The public package contains MIT-licensed source, two project-created procedural themes, tools, and synthetic test fixtures. Imported artwork, sounds, logos, sprite-derived collision masks, generated headers, screenshots, and asset-bearing firmware stay private. This is an independent project with no affiliation with the original game's creators or M5Stack.

## Build your copy

Use Python 3.11 or newer, [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/), and a C++ compiler for host tests. Optional audio conversion requires FFmpeg. The native importer also requires Python with Tkinter and a desktop display.

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

3. Open the native importer, choose a compatible local APK or atlas/manifest pair, and check the rights confirmation:

   ```sh
   python tools/import_gui.py
   ```

   The command-line equivalent for an atlas is:

   ```sh
   python tools/generate_assets.py --atlas /path/to/atlas.png --manifest /path/to/atlas.txt --acknowledge-rights
   ```

   Both paths require this statement: “I confirm I have the rights or permission needed to use and convert the assets I supply for this build.” It records your statement and grants no license. The importer makes no network requests. See [asset input details](assets/README.md) for compatible APKs, optional audio, and custom skins.

4. Build, then choose your device's serial port explicitly:

   ```sh
   python tools/build.py
   python tools/flash.py --port /dev/cu.YOUR_DEVICE
   ```

   Windows serial ports use names such as `COM3`. Building leaves the serial port closed. Close serial monitors before playing.

Imported files, generated headers, staged sketches, firmware, and release output stay under ignored `.local/`. Dependencies use the existing Arduino data directory and a private sketchbook. [Custom skin imports](assets/README.md#custom-skin-folders) can add up to ten local packs to a build.

## Buttons and menus

The front M5 button is **Action**: flap, start a round, or select the highlighted menu item. After a crash, wait for replay, press to return to Ready, then press again to start.

Tap the right side button once to open the menu. Tap it again to move through options. Press Action to select. Hold the right button for at least 0.7 seconds to exit from any menu page. Every page ends with Back. The menu pauses single-player play.

On the M5StickC Plus, the other side button controls power. Software can read its short presses through the power-management chip, while a long hold can switch the device off. It has no extra gameplay assignment in this build. [Official board documentation](https://docs.m5stack.com/en/core/m5stickc_plus) describes the two GPIO buttons and power key.

Brightness has five settings: Night, Dim, Medium, Bright, and Full. All keep the backlight on. The lowest setting uses the board's lowest nonzero brightness range; its physical visibility still needs a display check.

## Modes

| Mode | Behavior | Saved high score |
| --- | --- | --- |
| Original | Manual play with the original rules and your local base artwork. | Separate Original record. |
| Clock | Automated flight with time digits between the pipes. Action starts a manual round; autoplay returns after that round ends. | Excluded, including manual takeover. |
| Demo | An imperfect autopilot varies its skill, delays, and mistakes between runs. Action can take over for a round. | Excluded, including manual takeover. |
| Arcade | Larger ticket display, a narrowing gap, and virtual milestone rewards. | Separate Arcade record. |
| Big 2x | Enlarged character and obstacle sprites, larger collision geometry, and less grass. | Separate Big record. |
| Co-op | Two devices share a course and show each other's birds over BLE. | Excluded. |

Set clock time through **Set clock**. Tap the menu button to highlight Hour, Minute, or 24-hour, then press Action to increment or toggle it. The device uses its RTC when available. An unset RTC starts from the build computer's local time. Adjust it after flashing if needed. Setting the clock is local and requires no network account.

Arcade awards one virtual ticket per cleared pipe, plus bonuses of 10, 20, 50, 500, and 1000 when you reach pipes 10, 20, 35, 50, and 60. Ten cleared pipes therefore give twenty virtual tickets. This formula is our handheld adaptation. Virtual tickets have no redemption value. [Arcade research](docs/ARCADE_RESEARCH.md) documents manufacturer manuals, observed gameplay, and the physics details those sources leave unknown.

For a race, open **Co-op link**, choose **Host game** on one device and **Join game** on the other, then press Action on both to get ready. The opponent receives a 180-degree hue shift, with inversion for art close to gray. Opening the menu ends the connection. See [pairing, timing, and verification details](docs/COOP.md). A complete two-device radio playthrough still needs a physical test.

## Skins and display fit

The Skins menu starts with ten project palette presets: Original, Midnight, Arctic, Desert, Candy, Neon, Mono, Ocean, Pumpkin, and Retro. Two further themes, **Toast on the Run** and **Past Due**, use project-created procedural graphics. Imported custom packs appear after those choices.

The [ten curated author packs](docs/SKINS_RESEARCH.md) are optional art sources with different licenses. Their [metadata catalog](assets/skin_catalog.json) contains links and rights notes. They are separate from the built-in palettes, require local preparation, and have no claimed popularity ranking.

The renderer uses a 144 by 256 pixel canvas. A 135 by 240 display crops 4 pixels from the left, 5 from the right, and 8 from the top and bottom, preserving the sprite grid. An 80 by 160 display fits the scene at 80 by 142 with nearest-neighbor sampling. The base atlas importer verifies duplicated 2 by 2 blocks before reducing compatible sprites.

## Tests and source distribution

```sh
python tools/test.py
python tools/check_distribution.py --git
python tools/package_source.py --git
```

With an extracted source ZIP, omit `--git`. A Git checkout is audited automatically, including reachable history. Tests use independent geometric assets and exercise import, gameplay, application state, rendering, the co-op protocol, and publication checks. Hardware checks of RTC retention, display brightness, and a complete two-device race remain separate.

The source exporter writes `.local/releases/m5-flap-source.zip` from the explicit [publication list](PUBLIC_FILES.txt). [Distribution instructions](docs/DISTRIBUTION.md) explain the checks and exclusions. These tools support review and cannot establish ownership or legal clearance.

The code uses the [MIT license](LICENSE) with [upstream notices](THIRD_PARTY_NOTICES.md). Imported assets retain their own rights. Archive.org availability, an uploader's credit, or possession of an APK does not establish permission for your intended use. Read the [rights boundary](docs/RIGHTS.md) before distributing a build.

## Local storage

Original development media, generated headers, firmware, device backups, and machine-specific notes were moved into ignored `.local/` during source preparation. Nothing was uploaded. Installed local toolchains stay ignored under `toolchain/`. Test executables and temporary build caches can be removed and recreated with the commands above. Preserve private input files you cannot recover before deleting `.local/`.

After the feature build, temporary compiler objects, staged sketches, test executables, and used dependency downloads were removed. Recreate them with `python tools/build.py` and `python tools/test.py`. The verified private firmware files remain under `.local/releases/private-firmware/` for later device testing.
