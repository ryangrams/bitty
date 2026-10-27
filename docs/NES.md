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
| 1, MMC1 | Power-of-two PRG bank counts from 16 to 256 KiB, with power-of-two CHR ROM sizes from 8 to 128 KiB or 8 KiB volatile CHR RAM. |
| 37, MMC3 multicart variant | 256 KiB PRG and 256 KiB CHR. The cartridge's own menu remains part of the imported image. |
| 118, TxSROM | Power-of-two PRG sizes from 32 to 512 KiB and CHR ROM sizes from 8 to 256 KiB. Two nametable pages are required. |
| 4, MMC3 | Included in the compiled emulator core as the basis for mappers 37 and 118; direct importer support is currently disabled. |

Trainers, NES 2.0 submappers, exponent ROM sizes, Dendy timing, console extensions, miscellaneous ROM sections and unsupported RAM declarations are rejected. The core provides 8 KiB of cartridge SRAM where supported. Its contents are held in memory and do not persist across power cycles, including for cartridges with a battery flag. There is no save-state UI.

PAL cartridges select a 50-frame-per-second timing target. NTSC and multi-region cartridges select 60. These are emulation targets; sustained speed and audio timing on the physical ESP32 remain unverified.

Arduino builds compose and send video every third emulated frame by default. CPU execution, audio, sprite status and mapper interrupts still advance on every emulated frame. `NES_VIDEO_DIVISOR` can override this display interval. It defaults to 1 in desktop tests. Reducing video refresh helps the ESP32 keep up; measured hardware speed is still being checked.

## Screen and controls

NES play rotates the display to landscape. The recognized Mario multicart revision starts automatically behind an M5 startup screen. Its native layout requires an exact 524,304-byte image with CRC-32 `4AA87799`; other cartridge images use the general scaled 4:3 view with side bars.

On a 240×135 display, the native Mario view copies NES columns 8 through 247 at one display pixel per source pixel. The first 16 output rows show source rows 16 through 31 for the HUD. The remaining 119 rows show a world window whose top follows Mario between source rows 32 and 121. The camera moves vertically to keep his visible sprite inside that window. This crop preserves the source pixels and trims the outer columns and part of the vertical view. The M5 menus return to portrait.

[Tetris](TETRIS.md) has one launcher entry with **Solo** and **2 players** settings. Solo uses its private TetrisGYM image; two-player play uses its private Tetris Zero image. Both use portrait layouts and M5 settings. You can include those two images and the general NES image in one build, alongside Flappy Bird.

Use a paired controller for A, B and directions. A and B retain their NES meanings, including jump and run in Super Mario Bros. All D-pad and A/B combinations pass through to the general NES game after automatic startup. Dedicated controller Start, Select and Menu buttons keep their assignments. Use **Game Settings > Restart game** to repeat Mario's automatic launch.

Release the front M5 button before 700 ms (0.7 seconds) to send Start. Hold it for at least 700 ms to send Select once. Hold the right side button for 700 ms to open the M5 menu. While the menu is open, tap right to advance, press front to select, or hold right to exit. These physical buttons supply Start, Select and menu access for controllers such as the IINE Gamebrick Mini. See [pairing and the complete control table](CONTROLLERS.md).

## Sound

On the StickC Plus, the firmware drives the passive buzzer on GPIO2 with ESP32 LEDC tones. It approximates one audible NES pulse-channel voice at a time and adds project-authored cues for actions such as jumping, rotating and clearing lines. This output needs no imported audio assets. The available volume settings are Off, 20%, 40%, 60%, 80% and Full, with 40% as the initial default. Off stops playback immediately.

The emulator retains its audio timing and PCM output for compatible speaker hardware. The Plus buzzer produces a monophonic approximation. Audibility, sound balance and volume still need a physical listening test. See the [board and audio notes](../README.md#buttons-and-menus).

## Emulator dependency and port

The build fetches [PeakRacing/nes at revision `72326d82d642c78717a111629c8e04dbae39e8d0`](https://github.com/PeakRacing/nes/tree/72326d82d642c78717a111629c8e04dbae39e8d0), licensed under Apache 2.0. `tools/setup_nes.py` verifies the SHA-256 of every selected upstream file and prepares the dependency under ignored `.local/nes-deps/`. It can be prepared separately or from verified cached files:

```sh
python tools/setup_nes.py
python tools/setup_nes.py --offline
```

The local port adds a cooperative single-frame entry point, preserves fractional timing across frames, retains mappers 0/1/4/37 and adds mapper 118, uses 8-row RGB565 strips (4,096 bytes), lowers audio output to 24 kHz, and guards a trainer helper for filesystem-free builds. Skipped video frames retain sprite-zero collision checks and mapper fetch side effects. OAM sprite selection is cached per scanline and invalidated on frame, OAM write, DMA and height changes. Modified upstream files carry a change notice. The private dependency folder retains the upstream license, a modification notice and a receipt. See [third-party notices](../THIRD_PARTY_NOTICES.md).

Run `python tools/test.py --nes` for the optional host checks. They use a small synthetic cartridge program and test CPU execution, video strips, audio, timing and controller registers with video divisors 1 and 3. A separate comparison covers 65,536 scroll and sprite-zero cases, mapper fetches and sprite overflow. Private host runs have exercised the supplied PAL mapper 37 cartridge, Mario's automatic launch, movement, jumping and exact crop pixels. Its ROM bytes and captures remain private. Display fit and playability of the native Mario view, controller responses and sustained speed still need physical verification.
