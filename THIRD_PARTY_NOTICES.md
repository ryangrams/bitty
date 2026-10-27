# Third-party notices

The [project MIT license](LICENSE) covers Ryan Grams's contributions to M5 Flap. Adapted code retains the upstream notices below. Third-party dependencies retain their own licenses. Imported game artwork, audio, logos, fonts, ROMs and generated derivatives receive no license from this project. See [Rights and distribution](docs/RIGHTS.md).

## FlapPyBird gameplay reference

`firmware/flappy/game_core.h` adapts gameplay behavior from [sourabhv/FlapPyBird, commit `4fc78596c9f4abf0071a5490734cee1c33fd4fa0`](https://github.com/sourabhv/FlapPyBird/tree/4fc78596c9f4abf0071a5490734cee1c33fd4fa0). The implementation has been reworked for C++ and fixed simulation ticks.

That revision carries the MIT license. Its [license text](LICENSES/FlapPyBird-MIT.txt) is retained verbatim, including the upstream placeholder copyright line. The source repository is credited to its GitHub author, `sourabhv`. This code attribution grants no rights in the original game's artwork or audio.

## M5StickC display power reference

The AXP192 rail initialization in `firmware/flappy/stick_power.h` follows [m5stack/M5StickC, `src/AXP192.cpp`](https://github.com/m5stack/M5StickC/blob/master/src/AXP192.cpp).

Copyright (c) 2020 M5Stack. The upstream [MIT license](LICENSES/M5StickC-MIT.txt) is retained verbatim.

`firmware/flappy/stick_display.h` adapts the StickC and StickC Plus panel profiles and AXP192 backlight behavior from [M5GFX 0.2.28](https://github.com/m5stack/M5GFX/blob/0.2.28/src/M5GFX.cpp). Copyright (c) 2021 M5Stack. Its MIT notice is included in the header. The manual profiles bypass LCD identification reads when automatic detection fails.

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

`tools/setup_nes.py` fetches and checks selected files from the pinned PeakRacing/nes revision. Its [Apache 2.0 license text](LICENSES/PeakNES-Apache-2.0.txt) is retained verbatim. The script prepares a private dependency with a cooperative single-frame entry point, preserved fractional frame timing, a mapper 0/4/37 registry, RGB565 half-frame buffers, 24 kHz audio, and a filesystem guard around the trainer helper. Modified files receive a change notice. Upstream source notices, the license and a generated modification notice remain with the prepared dependency.

The emulator's software license grants no rights to NES cartridge images, game artwork, music, names or other imported content. Local ROMs and firmware containing them are excluded from the public source distribution.

## Names and affiliation

Flappy Bird identifies the original game and compatible asset format. M5Stack and ESP32 identify supported hardware and dependencies. M5 Flap is independently maintained and claims no affiliation with or endorsement by those projects or their rights holders. The project license grants no rights to third-party trademarks or branding.
