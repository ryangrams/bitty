# Geometry Dash

Play the 22 original Geometry Dash levels, Stereo Madness through Dash, through **Famidash v1.2.8**, the author's NES demake. The M5StickC Plus draws its own full-screen graphics from the cartridge's live state: brighter backgrounds that follow native color changes, yellow characters, white obstacle edges and procedural shapes for terrain, spikes, portals and other objects. Famidash supplies the adapted levels and NES game logic. The iPad game's artwork, engine and soundtrack remain separate from this adaptation.

Download **Famidash v1.2.8.nes**, the original/main release, from the [author's release page](https://kandowontu.itch.io/famidash). The [matching release source](https://github.com/tfdsoft/famidash/tree/0df680d0e02f0ccb438f1224d162a0ab073b7204) includes 22 original levels and 21 community levels. The M5 level selector exposes the 22 originals. The separate Huge Man, VS, Album and nightly builds use different layouts and fall outside this integration's supported input.

## Import and launch

```sh
python tools/import_rom.py --rom "/path/to/Famidash v1.2.8.nes" --title "Geometry Dash" --output-dir .local/geometry-rom --acknowledge-rights
python tools/build.py
python tools/flash.py --port /dev/cu.YOUR_DEVICE
```

The build finds `.local/geometry-rom/` automatically. Use `python tools/build.py --geometry-dir /path/to/private/import` for another folder. Your general NES cartridge, including Mario, keeps its own import folder and launcher entry.

Hold the right side button for 700 ms to open the M5 menu, then choose **Change Game > Geometry Dash**. **Game at Boot** can save it as your startup choice; Flappy Bird remains the initial default.

The launcher advances the cartridge's warning, title and level-selection screens automatically. Its first startup includes Famidash's compulsory credits, lasting about 15 seconds. The M5 shows a preparation screen during this sequence, then starts the selected level. Use **Game Settings > Level** to choose among the 22 original levels. Selecting a level closes the menu and starts that level. **Game Settings > Restart level** restarts the current selection.

The Geometry Dash slot requires the exact v1.2.8 main cartridge: **786,448 bytes**, CRC32 **`5BF705B3`**, SHA-256 **`7acf068a406d551ff18afa3fc1ed228ff90f72651c4a90a3862b997240bb9b58`**. The firmware checks its size and CRC32 before using release-specific RAM addresses.

## Screen and timing

Gameplay fills the StickC Plus's **240×135 landscape display**. World geometry uses **16-pixel blocks at a 1:1 pixel scale**. The viewport trims eight source columns from each side and follows the character vertically through jumps, landings and flight. The shorter view shows a portion of the cartridge's vertical playfield. The M5 menus use portrait orientation and the native-size menu font.

The procedural renderer paints directly into the display-sized RGB565 canvas. It preserves the scale of terrain and hazards without stretching the NES picture. The NES core skips RGB pixel composition while retaining CPU, APU, logical PPU and mapper behavior. Colors are prepared in display byte order as they are drawn, avoiding a full-frame conversion pass. The display worker sends the shared canvas through DMA while the main CPU advances the next game frame, and the canvas remains reserved until the transfer finishes.

The cartridge declares multiregion support; this launcher selects **NTSC 60 Hz timing**. Simulation timing remains independent of display transfer performance. Three probes of 500 completed SPI frame intervals took 8.334499, 8.367268 and 8.331629 seconds. Their combined 1,500 intervals took 25.033396 seconds: **59.9200 completed display frames per second**. Tests used Stereo Madness, Geometrical Dominator and Dash, Bluetooth LE scanning, Full volume and held action with native retries. A connected controller was unavailable for this measurement. Panel scanout, flight sections, upper camera behavior and complete level playthroughs remain unverified on hardware. Portable raster tests cover all nine forms, dual and mini states, gravity, camera limits and terrain classes.

## Controls

| Input | During gameplay |
| --- | --- |
| Front M5, without a controller | Press to jump; hold for sustained flight or the active form's held action. |
| Controller A or B | Jump or hold the active form's action. |
| Controller Start | Open Game Settings. |
| Front M5, with a controller | A short press and release opens Game Settings. |
| Hold right side 700 ms | Open or close the M5 menu. |
| Controller Menu | Open the M5 menu. |

Controller directions and Select are consumed during gameplay so they cannot activate Famidash's native menus or debug controls. Side-button taps have no gameplay action. In the M5 menus, tap right to highlight the next option and press front M5 to select. A controller's D-pad navigates and A selects. Every page ends with Back.

## Progress and sound

Famidash uses 8 KiB of cartridge SRAM. Progress remains in RAM while this cartridge stays loaded, including during battery sleep and soft resets for level changes or restarts. Switching games or powering off clears that state. Persistent progress across power cycles is not implemented.

The StickC Plus passive buzzer uses the existing monophonic NES approximation. It cannot reproduce the complete multichannel soundtrack. Volume remains under the M5 menu.

## Source and rights

The author's [MIT software license](../LICENSES/Famidash-MIT.txt) and [upstream credits](https://github.com/tfdsoft/famidash/blob/0df680d0e02f0ccb438f1224d162a0ab073b7204/README.md) accompany this integration's references. The M5 launcher, state adapter, input adapter and procedural renderer are project code. The public source package excludes Famidash's ROM, game implementation, graphics, music and generated ROM headers.

Importing records your rights acknowledgment and grants no license. Keep the downloaded cartridge and firmware containing it private unless you have the permissions needed for distribution. Famidash's software license does not establish permission to redistribute RobTop assets or third-party musical compositions. [RobTop's published policy](https://www.robtopgames.com/youtube.html) also distinguishes third-party rights. See the [project's rights boundary](RIGHTS.md).
