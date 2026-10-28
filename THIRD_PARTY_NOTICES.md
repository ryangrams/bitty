# Third-party notices

The [project MIT license](LICENSE) covers Ryan Grams's contributions to M5 Flap. Adapted code retains the upstream notices below. Third-party dependencies retain their own licenses. Imported game artwork, audio, logos, fonts, ROMs and generated derivatives receive no license from this project. See [Rights and distribution](docs/RIGHTS.md).

## FlapPyBird gameplay reference

`firmware/flappy/game_core.h` adapts gameplay behavior from [sourabhv/FlapPyBird, commit `4fc78596c9f4abf0071a5490734cee1c33fd4fa0`](https://github.com/sourabhv/FlapPyBird/tree/4fc78596c9f4abf0071a5490734cee1c33fd4fa0). The implementation has been reworked for C++ and fixed simulation ticks.

That revision carries the MIT license. Its [license text](LICENSES/FlapPyBird-MIT.txt) is retained verbatim, including the upstream placeholder copyright line. The source repository is credited to its GitHub author, `sourabhv`. This code attribution grants no rights in the original game's artwork or audio.

## M5StickC display power reference

The AXP192 rail initialization in `firmware/flappy/stick_power.h` follows [m5stack/M5StickC, `src/AXP192.cpp`](https://github.com/m5stack/M5StickC/blob/master/src/AXP192.cpp).

Copyright (c) 2020 M5Stack. The upstream [MIT license](LICENSES/M5StickC-MIT.txt) is retained verbatim.

`firmware/flappy/stick_display.h` adapts the StickC and StickC Plus panel profiles and AXP192 backlight behavior from [M5GFX 0.2.28](https://github.com/m5stack/M5GFX/blob/0.2.28/src/M5GFX.cpp). Copyright (c) 2021 M5Stack. Its MIT notice is included in the header. The manual profiles bypass LCD identification reads when automatic detection fails.

## Menu bitmap font

`firmware/flappy/menu_font.h` embeds M5 Menu Pixel, a printable ASCII subset of the 6x12 normal bitmap in [Terminus Font 4.49.1](https://terminus-font.sourceforge.net/). Copyright (C) 2020 Dimitar Toshkov Zhekov. The font remains under the [SIL Open Font License 1.1](LICENSES/Terminus-OFL-1.1.txt), copied verbatim from the official release. The subset uses its own name to respect the reserved name "Terminus Font".

The conversion retains all 95 printable ASCII glyphs, their complete 6x12 cells and six-pixel advance. It shifts BDF byte padding into six-bit row storage without changing any glyph pixel. Menus and message screens draw these pixels at native size. The optional integer 2x renderer repeats each complete pixel in a 2x2 block.

The [official source archive](https://sourceforge.net/projects/terminus-font/files/terminus-font-4.49/terminus-font-4.49.1.tar.gz/download) has SHA-256 `d961c1b781627bf417f9b340693d64fc219e0113ad3a3af1a3424c7aa373ef79`. Its `ter-u12n.bdf` has SHA-256 `9e942cad8173fddf34fc8a784e9ec6d2d4cfb8b82363faf65f43b236fe2f5a99`. The font test checks every stored glyph through a checksum and verifies direct native and integer 2x drawing.

## Build dependencies

The public source distribution excludes installed libraries, toolchains and compiled firmware. The setup process obtains dependencies separately.

| Dependency | Version | Upstream license information |
| --- | --- | --- |
| M5Unified | 0.2.21 | [MIT, Copyright (c) 2021 M5Stack](https://github.com/m5stack/M5Unified/blob/0.2.21/LICENSE) |
| M5GFX | 0.2.28 | [MIT, Copyright (c) 2021 M5Stack](https://github.com/m5stack/M5GFX/blob/0.2.28/LICENSE), with additional notices in bundled components |
| NimBLE-Arduino | 2.3.6 | [Apache 2.0](https://github.com/h2zero/NimBLE-Arduino/blob/2.3.6/LICENSE), with component and per-file notices |
| Arduino core for ESP32 | 3.3.11 | [LGPL 2.1 license text](https://github.com/espressif/arduino-esp32/blob/3.3.11/LICENSE.md); [core headers specify LGPL 2.1 or later](https://github.com/espressif/arduino-esp32/blob/3.3.11/cores/esp32/Arduino.h); bundled components retain their own licenses |
| PeakRacing/nes | `72326d82d642c78717a111629c8e04dbae39e8d0` | [Apache 2.0, Copyright PeakRacing](https://github.com/PeakRacing/nes/blob/72326d82d642c78717a111629c8e04dbae39e8d0/LICENSE) |

M5GFX contains LovyanGFX-derived files and other third-party material with separate notices. Preserve the installed libraries' license files and per-file notices when redistributing them. This table does not replace those terms.

Firmware distribution also carries the obligations of its linked libraries, including applicable LGPL source and relinking requirements. A build containing imported artwork, audio or ROMs requires separate rights for those inputs. The project does not provide such a firmware release.

## NES core modifications

`tools/setup_nes.py` fetches and checks selected files from the pinned PeakRacing/nes revision. Its [Apache 2.0 license text](LICENSES/PeakNES-Apache-2.0.txt) is retained verbatim. The script prepares a private dependency with a cooperative single-frame entry point, preserved fractional frame timing, a mapper 0/1/4/37/118 registry, 8-row RGB565 strip buffers, 24 kHz audio, and a filesystem guard around the trainer helper.

Runtime video intervals can skip composition and display callbacks while retaining CPU, APU, sprite status and mapper timing. A copied 240-bit source-row mask limits RGB work while preserving offscreen PPU fetches and frame callbacks; the two-window API fills this mask. An empty mask suppresses RGB composition for the native Geometry renderer while retaining the core's logical state. The opacity shortcut retains full mapper fetches when they have side effects. A scanline sprite cache preserves OAM ordering, overflow and pattern fetches, with invalidation on frame start, OAM writes, DMA and sprite-height changes.

Optional PCM suppression retains channel phase, envelopes, length counters, noise state, DMC bus reads and interrupt behavior. An optional fixed-window PRG read cache preserves the mapper's original ROM pointers. Guarded NROM self-jump batching combines repeated idle instructions within one CPU budget, preserving consumed cycles and interrupt boundaries. It requires a stable self-jump in ROM, excludes mapper CPU-clock and PRG-read hooks, and retains the unbatched path when its interrupt guards apply.

Modified files receive a change notice. Upstream source notices, the license and a generated modification notice remain with the prepared dependency. The public project contains the preparation script and synthetic verification programs. The fetched upstream implementation stays outside the source package.

The emulator's software license grants no rights to NES cartridge images, game artwork, music, names or other imported content. Local ROMs and firmware containing them are excluded from the public source distribution.

## TetrisGYM compatibility references

The RAM addresses, stage values and screen regions in `firmware/flappy/tetris_gym.h` and `firmware/flappy/tetris_viewport.h` refer to [TetrisGYM 6.0.0](https://github.com/kirjavascript/TetrisGYM/releases/tag/6.0.0), commit [`f487037746c9ff3d6e679fb7e4381e9264364bac`](https://github.com/kirjavascript/TetrisGYM/tree/f487037746c9ff3d6e679fb7e4381e9264364bac). The C++ wrappers use these compatibility facts to configure a privately loaded cartridge and arrange the cartridge's rendered pixels on the M5 screen.

The public source package excludes the original game, TetrisGYM cartridge code and disassembly, patch files, graphics, audio and generated ROM headers. The project license covers its own wrappers and local BPS and IPS tools. It grants no rights to the base game, TetrisGYM patch or their content. See the [private Tetris patch workflow](docs/TETRIS.md) for the local input boundary.

## Tetris Zero compatibility references

`firmware/flappy/tetris_zero.h` and `firmware/flappy/tetris_zero_viewport.h` describe menu stages, player state addresses and screen regions used by the mapper 118 v2 version of Tetris Zero. The [author's project entry](https://www.romhacking.net/hacks/1013/) identifies the hack. The wrappers use those compatibility facts with a privately supplied cartridge. The public package contains no patch, cartridge code, disassembly, save file, graphics or audio from that project. Obtain and review the patch author's terms separately.

## Famidash compatibility references

The Geometry Dash launcher accepts the author's privately imported [Famidash v1.2.8 main release](https://kandowontu.itch.io/famidash). Its compatibility metadata refers to the [v1.2.8 release source, commit `0df680d0e02f0ccb438f1224d162a0ab073b7204`](https://github.com/tfdsoft/famidash/tree/0df680d0e02f0ccb438f1224d162a0ab073b7204), including the original/main level order, MMC3 cartridge declaration, RAM addresses, level stream format and collision classes. `firmware/flappy/geometry_collision.h` adapts its point predicates under that MIT license. The read-only level decoder implements the documented release format and reads level bytes from the privately supplied cartridge. The upstream MIT notice, Copyright 2023-2024 TFDSoft, is retained verbatim in [LICENSES/Famidash-MIT.txt](LICENSES/Famidash-MIT.txt).

The launcher, `firmware/flappy/geometry_input.h`, `firmware/flappy/geometry_scene.h`, `firmware/flappy/geometry_level.h`, `firmware/flappy/geometry_launcher.h` and `firmware/flappy/geometry_renderer.h` are project code. The procedural renderer uses its own RGB565 shapes and colors, reads live state from the verified cartridge and leaves game logic to Famidash. The public source package excludes Famidash's cartridge image, level data, graphics, music and generated ROM headers. The software license grants no rights in RobTop assets or third-party musical compositions. The [private import workflow](docs/GEOMETRY_DASH.md) keeps these inputs separate from the source distribution.

## Names and affiliation

Jetpack Joyride identifies Halfbrick Studios' original game. The native M5 game uses project-created code and art. [timstrasser's Flipper remake](https://github.com/timstrasser/flipper-jetpack-game) is credited as a low-resolution visual and gameplay reference. At review, that repository declared no license. Its source and assets are excluded from this project. Halfbrick's original art, music and mobile application are also excluded. See [the reference notes](docs/JETPACK_RESEARCH.md).

The optional [Jetpack importer](docs/JETPACK_IMPORT.md) reconstructs coin formations and, when explicitly selected, a small runner sprite pack from a package supplied locally by the builder. Those generated derivatives and firmware containing them stay private. The importer ships no original game data and downloads none. Its acknowledgement records the builder's confirmation and grants no rights to the source package or its contents.

Flappy Bird identifies the original game and compatible asset format. M5Stack and ESP32 identify supported hardware and dependencies. Bitty is independently maintained and claims no affiliation with or endorsement by those projects or their rights holders. The project license grants no rights to third-party trademarks or branding.

## Browser installer

The generated installer bundles ESP Web Tools 10.4.0 (Apache-2.0), esptool-js and their locked browser dependencies. Their notices and available license texts accompany the built site. The source package carries the dependency lock and build procedure. The flasher uses preconfigured ESP32 image headers and a manifest that leaves Wi-Fi provisioning disabled. See https://github.com/esphome/esp-web-tools and https://github.com/espressif/esptool-js.
