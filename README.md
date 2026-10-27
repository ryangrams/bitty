# M5 Flap

A small game launcher for ESP32-based M5StickC devices. Play Flappy Bird modes, connect a Bluetooth LE controller, or load a local NES cartridge image. Flappy Bird starts by default. Original mode uses compatible artwork and optional sound that you supply locally.

The public package contains project source, two project-created procedural themes, tools, synthetic test fixtures, and upstream license notices. Imported artwork, sounds, logos, ROMs, generated headers, screenshots, and firmware containing imported material stay private. The NES core is fetched separately at a pinned revision. This is an independent project with no affiliation with the original games' creators or M5Stack.

For the IINE Gamebrick Mini, start with **H mode**, its yellow-light keyboard profile named **IINE-keyboard**. Hold its Pairing button for three seconds, then select **Controller > Pair controller** on the M5. See [controller setup and controls](docs/CONTROLLERS.md). Physical pairing with this controller remains unverified.

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

4. Optionally import a NES ROM you have the rights or permission to use:

   ```sh
   python tools/import_rom.py --rom /path/to/game.nes --title "My NES game" --acknowledge-rights
   ```

   This records your rights statement and grants no license. The ROM importer accepts local files and writes private output under `.local/nes-rom/`. See [NES formats, timing, and build options](docs/NES.md).

5. Build, then choose your device's serial port explicitly:

   ```sh
   python tools/build.py
   python tools/flash.py --port /dev/cu.YOUR_DEVICE
   ```

   Windows serial ports use names such as `COM3`. Building leaves the serial port closed. Close serial monitors before playing.

Imported files, generated headers, staged sketches, firmware, and release output stay under ignored `.local/`. Dependencies use the existing Arduino data directory and a private sketchbook. The build prepares the pinned NES core in `.local/nes-deps/` and includes `.local/nes-rom/` when present. [Custom skin imports](assets/README.md#custom-skin-folders) can add up to ten local packs to a build.

## Buttons and menus

In Flappy Bird, the front M5 button is **Action**: flap, start a round, or select the highlighted menu item. After a crash, wait for replay, press to return to Ready, then press again to start. In NES play, the front button sends Start.

Tap the right side button once to open the menu. Tap it again to move through options. Press Action to select. Hold the right button for at least 0.7 seconds to exit from any menu page. Every page ends with Back. The menu pauses single-player play.

The first two menu choices are **Game at Boot**, which saves your startup choice, and **Change Game**, which switches immediately. The initial startup choice is Flappy Bird. A build with a local ROM shows its title as the NES choice. A build without one shows **NES: no ROM** and keeps that choice unavailable. Menus use portrait orientation; NES play uses landscape with a 4:3 picture and side bars.

A connected controller's A button selects or flaps. Its D-pad moves through menus, and B provides the menu button in Flappy Bird. NES uses the controller's A, B, D-pad, Start and Select inputs. The IINE's missing buttons have combinations described in [controller controls](docs/CONTROLLERS.md#controls).

On the M5StickC Plus, the other side button controls power. Software can read its short presses through the power-management chip, while a long hold can switch the device off. It has no extra gameplay assignment in this build. [Official board documentation](https://docs.m5stack.com/en/core/m5stickc_plus) describes the two GPIO buttons and power key.

Volume has six settings: Off, 20%, 40%, 60%, 80%, and Full. It starts at 40% and saves your selection across restarts. Changing the level in the menu previews a flap sound. Off stops current playback immediately. Clock and Demo autoplay stay quiet, while manual play uses the selected volume. These percentages describe the software volume setting.

Game sounds are included when you import local audio. The [StickC Plus has a passive buzzer](https://docs.m5stack.com/en/core/StickC-Plus), and the [original StickC lists a microphone](https://docs.m5stack.com/en/core/StickC). Audio output depends on the detected board. Playback still needs a listening test on the physical device.

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

Controller pairing and co-op share one Bluetooth radio. Starting either disconnects the other. The controller host supports BLE HID; Bluetooth Classic controller profiles are unsupported by this build. NES compatibility, controls, and current hardware verification limits are documented in [NES](docs/NES.md).

## Skins and display fit

The Skins menu starts with ten project palette presets: Original, Midnight, Arctic, Desert, Candy, Neon, Mono, Ocean, Pumpkin, and Retro. Two further themes, **Toast on the Run** and **Past Due**, use project-created procedural graphics. Imported custom packs appear after those choices.

The [ten curated author packs](docs/SKINS_RESEARCH.md) are optional art sources with different licenses. Their [metadata catalog](assets/skin_catalog.json) contains links and rights notes. They are separate from the built-in palettes, require local preparation, and have no claimed popularity ranking.

The renderer uses a 144 by 256 pixel canvas. A 135 by 240 display crops 4 pixels from the left, 5 from the right, and 8 from the top and bottom, preserving the sprite grid. An 80 by 160 display fits the scene at 80 by 142 with nearest-neighbor sampling. The base atlas importer verifies duplicated 2 by 2 blocks before reducing compatible sprites.

## Tests and source distribution

```sh
python tools/test.py
python tools/test.py --nes
python tools/check_distribution.py --git
python tools/package_source.py --git
```

With an extracted source ZIP, omit `--git`. A Git checkout is audited automatically, including reachable history. Tests use independent geometric assets and exercise import, gameplay, application state, rendering, HID reports, the co-op protocol, ROM validation, and publication checks. The optional `--nes` suite prepares the pinned core and runs a synthetic cartridge program through CPU, video, audio, timing and input checks. Physical controller pairing, NES speed and display output, RTC retention, sound and volume, and a complete two-device race require separate hardware checks.

The source exporter writes `.local/releases/m5-flap-source.zip` from the explicit [publication list](PUBLIC_FILES.txt). [Distribution instructions](docs/DISTRIBUTION.md) explain the checks and exclusions. These tools support review and cannot establish ownership or legal clearance.

Project code uses the [MIT license](LICENSE) with [upstream notices](THIRD_PARTY_NOTICES.md). The NES core uses Apache 2.0. Imported assets and ROMs retain their own rights. Archive.org availability, an uploader's credit, or possession of an APK or cartridge image does not establish permission for your intended use. Read the [rights boundary](docs/RIGHTS.md) before distributing a build.

## Local storage

Original development media, generated headers, firmware, device backups, and machine-specific notes were moved into ignored `.local/` during source preparation. Nothing was uploaded. Installed local toolchains stay ignored under `toolchain/`. Test executables and temporary build caches can be removed and recreated with the commands above. Preserve private input files you cannot recover before deleting `.local/`.

After the feature build, temporary compiler objects, staged sketches, test executables, and used dependency downloads were removed. Recreate them with `python tools/build.py` and `python tools/test.py`. The verified private firmware files remain under `.local/releases/private-firmware/` for later device testing.
