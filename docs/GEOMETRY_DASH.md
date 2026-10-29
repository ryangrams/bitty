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

Hold the right side button for 700 ms to open the M5 menu, then choose **Change Game > Geometry Dash**. The last selected game returns automatically after startup.

The launcher advances the cartridge's warning, title and level-selection screens automatically. It batches silent menu steps while the M5 shows a preparation screen. Batching stops at the first gameplay state, preserving the level's normal NTSC cadence. Use **Game Settings > Level** to choose among the 22 original levels. Selecting a level closes the menu and starts that level. **Game Settings > Restart level** restarts the current selection.

The Geometry Dash slot requires the exact v1.2.8 main cartridge: **786,448 bytes**, CRC32 **`5BF705B3`**, SHA-256 **`7acf068a406d551ff18afa3fc1ed228ff90f72651c4a90a3862b997240bb9b58`**. The firmware checks its size and CRC32 before using release-specific RAM addresses.

## Screen and timing

Gameplay fills the StickC Plus's **240×135 landscape display**. World geometry uses **16-pixel blocks at a 1:1 pixel scale**. Once the character enters, its usual position is 40 pixels from the left edge, leaving more room to see upcoming obstacles. A read-only decoder reads upcoming terrain from your supplied cartridge, so the forward view can extend beyond the native collision buffer. Previously streamed columns keep their absolute world positions, preventing reused buffer columns from appearing at the left edge.

The vertical camera keeps its position in world coordinates. Ordinary jumps stay inside a wide quiet zone, so they do not move the camera. Taller movement receives an eased follow with bounded acceleration. The cartridge's own vertical scrolling does not move this camera a second time. The shorter view shows a portion of the cartridge's vertical playfield. The M5 menus use portrait orientation and the native-size menu font.

The procedural renderer paints directly into the display-sized RGB565 canvas. Hazard artwork distinguishes full spikes, half spikes, repeated teeth, saw quadrants and diagonal hazards. Source-derived collision masks expose the native cube collision areas, and mini characters use the native anchor. It preserves the scale of terrain and hazards without stretching the NES picture. The NES core skips RGB pixel composition while retaining CPU, APU, logical PPU and mapper behavior. Colors are prepared in display byte order as they are drawn, avoiding a full-frame conversion pass. Geometry frames are stored in the panel's portrait scan order while remaining landscape on screen. The display worker sends this contiguous canvas through DMA while the main CPU advances the next game frame, and the canvas remains reserved until the transfer finishes. Completion wakes the producer directly.

On the supported StickC Plus panel, the worker qualifies two clean scan cycles and their row timing before using scan synchronization. It starts transfers after the beam has passed the first visible rows and checks that the scan reading is fresh before launch. Invalid readings, a timeout or excessive launch delay disable synchronization. The panel refresh clock remains unchanged.

The cartridge declares multiregion support; this launcher selects **NTSC 60 Hz timing**. The emulator advances NTSC game frames on a 60 Hz schedule. The renderer uses every frame, with no regular frame-skipping divisor. Completed SPI transfers and the panel's own scan cycle are measured separately. The panel scan period is about 16.8 ms, or 59.5 Hz, with its refresh clock unchanged.

Earlier Plus hardware probes used Stereo Madness, Geometrical Dominator and Dash, Bluetooth LE scanning, Full volume and held action with native retries. No controller connected during those probes. These measurements predate the current control and startup changes.

| Level | 500 completed SPI intervals | Completed SPI FPS |
| --- | ---: | ---: |
| Stereo Madness | 8.333656 s | 60.00 |
| Geometrical Dominator | 8.469372 s | 59.04 |
| Dash | 8.334976 s | 59.99 |

The Mario regression probe measured 500 completed SPI intervals in 8.333910 seconds (60.00 FPS). Scan synchronization was enabled at the end of the Geometrical Dominator probe. Its conservative launch-delay guard disabled synchronization during the other two Geometry probes. These transfer measurements do not establish tear-free panel output.

Portable raster tests cover all nine forms, dual and mini states, gravity, world-camera invariance, terrain classes and exact equivalence between both framebuffer layouts. Independent collision tests cover all 256 metatiles and verify that every fatal point from the cube-mode oracle is painted. A tile comparison checks 1,193,010 cells across all 22 authored levels, including chunk continuations. Native launch checks compare 10,544 cells with the cartridge's committed collision map. An object-stream check covers 334,102 horizontal positions across those levels.

These checks cover geometry, memory safety and presentation logic. Direct inspection of tearing, flight sections, upper camera behavior and complete level playthroughs remains pending. Battery sleep and wake also remain unverified physically.

## Controls

| Input | During gameplay |
| --- | --- |
| Front M5, with or without a controller | Press to jump; hold for sustained flight or the active form's held action. |
| Controller A or B | Jump or hold the active form's action. |
| Controller Start | Open Game Settings. |
| Hold right side 700 ms | Open or close the M5 menu. |
| Controller Menu | Open the M5 menu. |

Controller directions and Select are consumed during gameplay so they cannot activate Famidash's native menus or debug controls. Side-button taps have no gameplay action. In the M5 menus, tap right to highlight the next option and press front M5 to select. A controller's D-pad navigates and A selects. Every page ends with Back.

## Progress and sound

Famidash uses 8 KiB of cartridge SRAM. Progress remains in RAM while this cartridge stays loaded, including during battery sleep and soft resets for level changes or restarts. Switching games or powering off clears that state. Persistent progress across power cycles is not implemented.

The StickC Plus passive buzzer uses the existing monophonic NES approximation. It cannot reproduce the complete multichannel soundtrack. Volume remains under the M5 menu.

## Source and rights

The author's [MIT software license](../LICENSES/Famidash-MIT.txt) and [upstream credits](https://github.com/tfdsoft/famidash/blob/0df680d0e02f0ccb438f1224d162a0ab073b7204/README.md) accompany this integration's references. The M5 launcher, state adapter, input adapter and procedural renderer are project code. The public source package excludes Famidash's ROM, level data, graphics, music and generated ROM headers.

Importing records your rights acknowledgment and grants no license. Keep the downloaded cartridge and firmware containing it private unless you have the permissions needed for distribution. Famidash's software license does not establish permission to redistribute RobTop assets or third-party musical compositions. [RobTop's published policy](https://www.robtopgames.com/youtube.html) also distinguishes third-party rights. See the [project's rights boundary](RIGHTS.md).
