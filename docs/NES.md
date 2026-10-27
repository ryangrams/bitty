# NES cartridges

Import a local `.nes` cartridge image, build it into your private firmware, then choose its title through **Change Game**. **Game at Boot** saves the game to start after the next power-on. Flappy Bird is the initial default. Without an imported ROM, the NES choice reads **NES: no ROM** and cannot be selected.

## Import and build

```sh
python tools/import_rom.py --rom /path/to/game.nes --title "My NES game" --acknowledge-rights
python tools/build.py
```

The title must contain 1 to 40 UTF-8 bytes without control characters. The importer writes `.local/nes-rom/generated_rom.h`, a source copy at `.local/nes-rom/source/rom.nes`, and an import receipt with the cartridge metadata and hash. The build uses this folder automatically. To choose another private import folder:

```sh
python tools/import_rom.py --rom /path/to/game.nes --title "My NES game" --output-dir .local/another-rom --acknowledge-rights
python tools/build.py --rom-dir .local/another-rom
```

The required statement is: “I confirm I have the rights or permission needed to use and convert the ROM I supply for this build.” The acknowledgment records your statement and grants no license. Import only files you have the rights or permission to use. The importer accepts local files and provides no ROM downloads. ROM bytes, generated headers, source copies, import receipts and firmware containing them stay outside the public source package.

## Accepted formats

The importer validates standard iNES and NES 2.0 cartridges up to 1 MiB. The file must exactly match the PRG and CHR lengths in its header.

| Mapper | Import support |
| --- | --- |
| 0, NROM | 16 or 32 KiB PRG with 8 KiB CHR ROM, or 8 KiB volatile CHR RAM. |
| 37, MMC3 multicart variant | 256 KiB PRG and 256 KiB CHR. The cartridge's own menu remains part of the imported image. |
| 4, MMC3 | Included in the compiled emulator core as the basis for mapper 37; direct importer support is currently disabled. |

Trainers, NES 2.0 submappers, exponent ROM sizes, Dendy timing, console extensions, miscellaneous ROM sections and unsupported RAM declarations are rejected. The current port provides no persistent cartridge save storage or save-state UI.

PAL cartridges select a 50-frame-per-second timing target. NTSC and multi-region cartridges select 60. These are emulation targets; sustained speed and audio timing on the physical ESP32 remain unverified.

## Screen and controls

NES play rotates the display to landscape and fits the image into a 4:3 rectangle with side bars. The M5 menus return to portrait. Cartridge pixels are scaled to fit the small display.

Use a paired controller for A, B and directions. A and B retain their NES meanings, including jump and run in Super Mario Bros. On the IINE Gamebrick Mini, Up+A sends Start, Down+B sends Select, and Down+A+B opens the M5 menu. A+B remains available for gameplay. The front M5 button sends Start, and the right side button opens the menu. See [pairing and the complete control table](CONTROLLERS.md).

## Emulator dependency and port

The build fetches [PeakRacing/nes at revision `72326d82d642c78717a111629c8e04dbae39e8d0`](https://github.com/PeakRacing/nes/tree/72326d82d642c78717a111629c8e04dbae39e8d0), licensed under Apache 2.0. `tools/setup_nes.py` verifies the SHA-256 of every selected upstream file and prepares the dependency under ignored `.local/nes-deps/`. It can be prepared separately or from verified cached files:

```sh
python tools/setup_nes.py
python tools/setup_nes.py --offline
```

The local port adds a cooperative single-frame entry point, preserves fractional timing across frames, retains mappers 0/4/37, uses RGB565 half-frame buffers, lowers audio output to 24 kHz, and guards a trainer helper for filesystem-free builds. Modified upstream files carry a change notice. The private dependency folder retains the upstream license, a modification notice and a receipt. See [third-party notices](../THIRD_PARTY_NOTICES.md).

Run `python tools/test.py --nes` for the optional host checks. They use a small synthetic cartridge program and test CPU execution, video, audio, timing and controller registers. A separate host run has exercised a privately supplied PAL mapper 37 cartridge and its own menu. Its ROM bytes and captures remain private. Physical display fit, controller pairing, playability and sustained speed still need hardware verification.
