# M5 Flap

A small game launcher for ESP32-based M5StickC devices. Play Flappy Bird modes, connect a Bluetooth LE controller, or load local NES and Tetris cartridges. Flappy Bird starts by default. Original mode uses compatible artwork that you supply locally. Game and menu cues are synthesized by the firmware.

The public package contains project source, two project-created procedural themes, tools, synthetic test fixtures, and upstream license notices. Imported artwork, sounds, logos, ROMs, generated headers, screenshots, and firmware containing imported material stay private. The NES core is fetched separately at a pinned revision. This is an independent project with no affiliation with the original games' creators or M5Stack.

For the IINE Gamebrick Mini, start with **H mode**, its yellow-light keyboard profile named **IINE-keyboard**. Hold its Pairing button for three seconds, then select **Controller > Pair controller** on the M5. H-mode pairing, reconnect after flashing, Tetris movement and both rotations have been physically confirmed. Mario's A jump, B+Right run, and A jump while B+Right stays held also work. The IINE profile maps the captured Space and Enter reports to game A and B. Complete controller playthroughs and power-cycle coverage remain pending. See [controller setup and controls](docs/CONTROLLERS.md).

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

   The single Tetris launcher entry uses private TetrisGYM and Tetris Zero images for solo and two-player play. The [Tetris preparation command](docs/TETRIS.md#prepare-tetris-with-one-command) applies your local patches to your local base ROM and prepares `.local/tetris-rom/` and `.local/tetris-zero-rom/`. A build can include both Tetris engines and the general NES cartridge.

5. Build, then choose your device's serial port explicitly:

   ```sh
   python tools/build.py
   python tools/flash.py --port /dev/cu.YOUR_DEVICE
   ```

   Windows serial ports use names such as `COM3`. Building leaves the serial port closed. Close serial monitors before playing.

   If automatic LCD detection leaves a StickC Plus screen black, choose its display profile when building:

   ```sh
   python tools/build.py --display stickc-plus
   ```

   Use `--display stickc` for the original StickC and `--display auto` for automatic detection on other boards. An explicit choice is saved in private `.local/display-profile.json` and reused by later builds. `--display auto` also resets a saved manual choice. Build and flash again after changing the profile.

Imported files, generated headers, staged sketches, firmware, and release output stay under ignored `.local/`. Dependencies use the existing Arduino data directory and a private sketchbook. The build prepares the pinned NES core in `.local/nes-deps/` and includes `.local/nes-rom/`, `.local/tetris-rom/` and `.local/tetris-zero-rom/` when present. Use `--rom-dir`, `--tetris-dir` or `--zero-dir` to choose private import folders. [Custom skin imports](assets/README.md#custom-skin-folders) can add up to ten local packs to a build.

## Buttons and menus

In Flappy Bird, the front M5 button is **Action**: flap, start a round, or select the highlighted menu item. After a crash, wait for replay, press to return to Ready, then press again to start. In NES play, a short release of the front button sends Start. Hold it for at least 700 ms (0.7 seconds) to send Select once. Tetris uses the controls described below, with the front button always assigned to downward movement.

Hold the right side button for at least 700 ms (0.7 seconds) to open the menu during play. While the menu is open, tap the right button to move through options and press Action to select. Hold the right button again to exit. A dedicated controller Menu button opens the menu with a tap. Every page ends with Back. The menu pauses single-player play.

Home contains **Game at Boot**, **Change Game**, **Game Settings**, **Brightness**, **Volume**, **Controller** and **Back**. Game at Boot saves your startup choice; Change Game switches immediately. The three visible game choices are **Flappy Bird**, the general **NES** cartridge with its imported title, and **Tetris**. Missing cartridges remain unavailable. Flappy Bird is the initial startup choice.

Game Settings holds the selected game's options. Flappy Bird puts Mode, Skins, Set clock and Co-op link there. Selecting a skin returns immediately to play. Menus and Tetris use portrait orientation. General NES play uses landscape with a scaled 4:3 picture or the recognized Mario cartridge's native pixel view. See [NES display details](docs/NES.md#screen-and-controls).

A connected controller's A button selects or flaps. Its D-pad moves through menus, and B opens the menu in Flappy Bird. NES and Tetris use the controller's A, B and D-pad inputs, including simultaneous presses. Controller Start and Select retain their NES assignments in general NES play and open Game Settings in Tetris. Controller Menu opens the M5 menu. In general NES play, the front M5 button supplies Start on a short release and Select once on a 700 ms hold. See [controller controls](docs/CONTROLLERS.md#controls).

Tetris defaults to **Tilt move**: tilt to move left or right, short-tap the left power button to rotate counterclockwise, and short-tap right to rotate clockwise. **Tilt rotate** swaps these roles: the side buttons move the piece, and tilt rotates it. In both presets, tap front to move down one row or hold front to soft-drop. The front button keeps that assignment with a Bluetooth controller connected. Calibration is under **Game Settings > Controls**.

Tetris Game Settings offers **Players**, A-Type or B-Type, starting level, controls, Board size, Link and Start / restart. **Players > Solo** selects TetrisGYM, with levels 0 through 29. **Players > 2 players** selects Tetris Zero, with levels 0 through 9 and a portrait view of both wells. A Zero-only build can also run its native solo mode. Solo defaults to Large, with equal 11-pixel cells drawn in the cartridge's colors. Native 8-pixel cells are optional; two-player play uses 6-pixel cells to fit both wells.

The launcher defaults to NTSC 60 Hz. Use an NTSC Mario image so its physics and timers match that clock. TetrisGYM's Speed setting can explicitly select PAL 50 Hz and changes both the cartridge flag and emulator clock. Start / restart applies the selected game options. The simulation clock and LCD refresh rate are separate.

For local two-player Tetris, the M5 controls Player 1 and a connected Bluetooth controller controls Player 2. Controller Down taps move one row and holds continue soft drop; Up hard-drops; A and B rotate. A+B uses a dedicated Hold slot once until the piece locks. The first Hold consumes NEXT and advances the cartridge's randomizer; later exchanges leave NEXT unchanged. Restart clears Hold. This is an M5 frontend extension. Both IINE rotation buttons have been physically confirmed. Manual Up, Down, Hold and the new board layouts still need physical checks.

On Mario's title screen, Up, Down or B starts a new game. A+B sends the cartridge's native A+Start continue command, returning to the first stage of the last world reached. Requests made during Game Over wait for the title screen. Ordinary gameplay keeps its A/B combinations, including running jumps. See [Mario controls and camera](docs/NES.md#screen-and-controls).

The experimental solo link gives two devices the same piece order, host level and timing, compares score and lines, and sends no garbage. The two-player link sends both players' inputs to identical Tetris Zero emulators. Both link modes still need a complete physical playthrough with two devices. See [Tetris controls, settings and local patch builds](docs/TETRIS.md).

On the M5StickC Plus, the left side button also controls power. A long hold retains the hardware power function. [Official board documentation](https://docs.m5stack.com/en/core/m5stickc_plus) describes the two GPIO buttons and power key.

Volume has six settings: Off, 20%, 40%, 60%, 80%, and Full. It starts at 40% and saves your selection across restarts. Changing the level in the menu previews the selected volume. Off stops current playback immediately. Flappy Bird's Clock and Demo autoplay stay quiet; manual play and menus use the selected volume. These percentages describe the software volume setting.

The [StickC Plus has a passive buzzer](https://docs.m5stack.com/en/core/StickC-Plus). The firmware drives it on GPIO2 with the ESP32 LEDC tone generator and includes project-authored cues for games and menus. This buzzer path needs no imported audio assets. NES play approximates one audible pulse-channel voice and adds basic game cues. Optional imported Flappy Bird audio remains available through the speaker output on compatible hardware.

The [original StickC lists a microphone](https://docs.m5stack.com/en/core/StickC); the Plus buzzer path targets the StickC Plus. Other models need compatible audio output hardware. Sound and volume still need a listening test on the physical device.

Brightness has three settings: Low, Medium and High. All keep the backlight on. Medium is the default, and your selection is saved across restarts.

## Flappy Bird modes

| Mode | Behavior | Saved high score |
| --- | --- | --- |
| Original | Manual play with the original rules and your local base artwork. | Separate Original record. |
| Clock | Automated flight with time digits between the pipes. Action starts a manual round; autoplay returns after that round ends. | Excluded, including manual takeover. |
| Demo | An imperfect autopilot varies its skill, delays, and mistakes between runs. Action can take over for a round. | Excluded, including manual takeover. |
| Arcade | Larger ticket display, a narrowing gap, and virtual milestone rewards. | Separate Arcade record. |
| Big 2x | Enlarged character and obstacle sprites, larger collision geometry, and less grass. | Separate Big record. |
| Co-op | Two devices share a course and show each other's birds over BLE. | Excluded. |

Set clock time through **Game Settings > Set clock**. Tap the menu button to highlight Hour, Minute, or 24-hour, then press Action to increment or toggle it. The device uses its RTC when available. An unset RTC starts from the build computer's local time. Adjust it after flashing if needed. Setting the clock is local and requires no network account.

Arcade awards one virtual ticket per cleared pipe, plus bonuses of 10, 20, 50, 500, and 1000 when you reach pipes 10, 20, 35, 50, and 60. Ten cleared pipes therefore give twenty virtual tickets. This formula is our handheld adaptation. Virtual tickets have no redemption value. [Arcade research](docs/ARCADE_RESEARCH.md) documents manufacturer manuals, observed gameplay, and the physics details those sources leave unknown.

For a race, open **Game Settings > Co-op link**, choose **Host game** on one device and **Join game** on the other, then press Action on both to get ready. The opponent receives a 180-degree hue shift, with inversion for art close to gray. Opening the menu ends the connection. See [pairing, timing, and verification details](docs/COOP.md). A complete two-device radio playthrough still needs a physical test.

Controller pairing, Flappy Bird co-op and the Tetris link share one Bluetooth radio. Starting one disconnects the other. Opening the menu during either linked game ends the connection. Tetris uses separate service identifiers from Flappy Bird. The controller host supports BLE HID; Bluetooth Classic controller profiles are unsupported by this build. NES compatibility, controls, and current hardware verification limits are documented in [NES](docs/NES.md).

## Skins and display fit

The Skins menu shows a two-column grid with six animated bird previews at a time. Back stays at the bottom. Select a card to apply that skin and return immediately to play.

The choices start with ten project palette presets: Original, Midnight, Arctic, Desert, Candy, Neon, Mono, Ocean, Pumpkin, and Retro. Two further themes, **Toast on the Run** and **Past Due**, use project-created procedural graphics. Imported custom packs appear after those choices.

The [ten curated author packs](docs/SKINS_RESEARCH.md) are optional art sources with different licenses. Their [metadata catalog](assets/skin_catalog.json) contains links and rights notes. They are separate from the built-in palettes, require local preparation, and have no claimed popularity ranking.

The renderer uses a 144 by 256 pixel canvas. A 135 by 240 display crops 4 pixels from the left, 5 from the right, and 8 from the top and bottom, preserving the sprite grid. An 80 by 160 display fits the scene at 80 by 142 with nearest-neighbor sampling. The base atlas importer verifies duplicated 2 by 2 blocks before reducing compatible sprites.

## Tests and source distribution

```sh
python tools/test.py
python tools/test.py --nes
python tools/check_distribution.py --git
python tools/package_source.py --git
```

With an extracted source ZIP, omit `--git`. A Git checkout is audited automatically, including reachable history. Tests use independent geometric assets and exercise import, gameplay, application state, rendering, HID reports, the link protocol, ROM validation, BPS and IPS patch checks and publication checks. The optional `--nes` suite prepares the pinned core and runs a synthetic cartridge program through CPU, video, audio, timing and input checks. Physical checks confirm H-mode pairing and reconnect after flashing, Tetris movement and A/B rotations, and Mario's jump, run and combined running jump. Tetris Up, Down, Hold and board layouts, Mario title shortcuts and the revised camera, power-cycle coverage, complete playthroughs, RTC retention, and sound and volume still need hardware verification. A second M5 is unavailable, so complete two-device games remain untested on hardware.

NES video uses 8-row RGB565 strips, keeping its pixel buffer at 4 KiB. Display composition can skip frames independently of the simulation. CPU, audio and cartridge timing advance on every emulated frame. A 60 Hz simulation setting does not guarantee 60 LCD refreshes per second. The measured defaults use 30 source frames per second for Mario and solo Tetris, and 20 for two-player Tetris, while every game targets a 60 Hz simulation clock. A connected-controller device probe measured 500 Mario game frames in 8.333 seconds. The optional NES checks compare sprite-zero behavior across 65,536 scroll and sprite cases.

The source exporter writes `.local/releases/m5-flap-source.zip` from the explicit [publication list](PUBLIC_FILES.txt). [Distribution instructions](docs/DISTRIBUTION.md) explain the checks and exclusions. These tools support review and cannot establish ownership or legal clearance.

Project code uses the [MIT license](LICENSE) with [upstream notices](THIRD_PARTY_NOTICES.md). The NES core uses Apache 2.0. Imported assets and ROMs retain their own rights. Archive.org availability, an uploader's credit, or possession of an APK or cartridge image does not establish permission for your intended use. Read the [rights boundary](docs/RIGHTS.md) before distributing a build.

## Local storage

Original development media, generated headers, firmware, device backups, and machine-specific notes were moved into ignored `.local/` during source preparation. Nothing was uploaded. Installed local toolchains stay ignored under `toolchain/`. Test executables and temporary build caches can be removed and recreated with the commands above. Preserve private input files you cannot recover before deleting `.local/`.

After the feature build, temporary compiler objects, staged sketches, test executables, and used dependency downloads were removed. Recreate them with `python tools/build.py` and `python tools/test.py`. The verified private firmware files remain under `.local/releases/private-firmware/` for later device testing.
