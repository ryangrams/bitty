# Tetris on M5

The launcher has one **Tetris** entry. **Game Settings > Players** selects solo or two-player play. Solo uses [TetrisGYM 6.0.0](https://github.com/kirjavascript/TetrisGYM/releases/tag/6.0.0), and two-player play uses Tetris Zero. Each engine keeps its own private patched cartridge image. The M5 frontend chooses the engine and runs its cartridge menus automatically.

Build both patched images from your own compatible local base ROM and the respective patches, then import them into the private folders below. The repository includes the frontend, emulator integration and patching tools. Obtain the game content and patches separately.

## Prepare Tetris with one command

Supply your local base ROM, the TetrisGYM 6.0.0 BPS patch and the Tetris Zero MMC3_118_v2 IPS patch:

```sh
python tools/prepare_tetris.py --rom /path/to/base.nes --gym-patch /path/to/gym.bps --zero-patch /path/to/zero.ips --acknowledge-rights
python tools/build.py
```

The preparation command checks every requested result before changing the build inputs. It requires the supported full-file CRC32 values: `37a5ec63` for TetrisGYM 6.0.0 and `66462642` for Tetris Zero MMC3_118_v2. These checks protect the cartridge-specific memory layouts used by the frontend. Other patch versions are rejected.

BPS checksums validate the patch, expected source and result. The helper permits a change to the NES header's mirroring bit only when that change matches the BPS patch's exact source checksum. IPS instruction bounds are checked, followed by the Zero result's version check. The original ROM and patch files remain unchanged.

The command writes standard private imports to `.local/tetris-rom/` and `.local/tetris-zero-rom/`. Each contains the generated firmware header, patched cartridge copy, import receipt, patch receipt and preparation receipt with version checks and source, patch and result hashes. All remain excluded from the public source package.

Supply either patch alone to prepare only that engine. A previously imported image for the other engine stays in place. At least one patch and `--acknowledge-rights` are required. The acknowledgment records your statement and grants no license.

Use `--output-root /path/to/private-folder` to place both imports elsewhere. Build those imports with `--tetris-dir /path/to/private-folder/tetris-rom --zero-dir /path/to/private-folder/tetris-zero-rom`. Keep original inputs outside the generated import folders. Existing imports are replaced only after staging succeeds; an interrupted or failed update restores the previous imports when recovery succeeds.

The individual patch and import commands below remain available for inspecting each step.

## Apply a local BPS patch

```sh
python tools/apply_bps.py --rom /path/to/base.nes --patch /path/to/patch.bps --output .local/tetris/patched.nes
```

The tool verifies the patch checksum, the expected source checksum, and the completed target checksum. It rejects truncated instructions, invalid copy ranges, and source, patch or target files larger than 1 MiB. The original ROM and patch stay unchanged. The output and its receipt must use paths separate from both inputs.

A successful run writes the patched image and a sibling `.patch-receipt.json` file. The receipt records SHA-256 hashes for the original source, patch and result, along with the BPS source and target checksums.

Some otherwise compatible NES source images differ in the header's mirroring bit. To permit that specific adjustment:

```sh
python tools/apply_bps.py --rom /path/to/base.nes --patch /path/to/patch.bps --output .local/tetris/patched.nes --nes-mirroring-header
```

This option tries flipping only bit 0 of header byte 6. The adjusted image must match the patch's exact expected source checksum before the patch can run. The receipt records any adjustment, and the original input remains unchanged. All other source mismatches still fail.

## Apply a local IPS patch

For a patch supplied in IPS format:

```sh
python tools/apply_ips.py --rom /path/to/base.nes --patch /path/to/patch.ips --output .local/tetris/patched.nes
```

The tool handles literal and repeated-byte records plus an optional final resize. It validates instruction lengths and keeps the source, patch and result within 1 MiB. The original inputs remain unchanged, and output or receipt paths that alias either input are rejected.

IPS has no embedded source or result checksum. Add `--source-sha1` followed by an independently verified 40-character source hash to require an exact base-file match. The tool performs no automatic NES header adjustment. Its `.patch-receipt.json` records the source, patch and result SHA-256 hashes, source and result SHA-1 hashes, and any supplied source expectation. Keep this receipt with your private build inputs.

## Import and build

```sh
python tools/import_rom.py --rom .local/tetris/patched.nes --title "TetrisGYM" --output-dir .local/tetris-rom --acknowledge-rights
python tools/build.py
```

The build includes `.local/tetris-rom/` when present. Use `--tetris-dir /path/to/private-import` to choose another imported Tetris folder. Base artwork and the other build dependencies still follow the [main build instructions](../README.md#build-your-copy). Display profiles such as `--display stickc-plus` remain available.

Choose **Tetris** through **Change Game** to switch immediately, or save it through **Game at Boot** for the next power-on. Flappy Bird remains the initial startup choice. The selected player mode is saved separately from the boot-game choice. Older builds' Tetris Zero boot choice migrates to the single Tetris entry.

The portrait layout requires a display at least 131 by 240 pixels, including the 135 by 240 StickC Plus screen. The frontend presents game settings on the M5 and launches the cartridge automatically. During play it arranges the playfield, Hold, NEXT and counters in portrait.

## Game settings

Open **Game Settings** while Tetris is selected:

| Setting | Choices |
| --- | --- |
| Players | Solo, or 2 players. Switching changes the active engine immediately. |
| Game type | A-Type or B-Type. Solo uses TetrisGYM's type 0 or type 6. |
| Starting level | 0 through 29 for TetrisGYM; 0 through 9 for Tetris Zero. |
| Controls | Tilt move, Tilt rotate, and tilt calibration. |
| Speed | Solo TetrisGYM: NTSC 60 Hz by default, or PAL 50 Hz by choice. This sets both the cartridge's timing flag and emulator clock. |
| Link | Disconnect, Host or Join. Solo uses same-piece competition; two-player mode uses the shared native game. |
| Board size | Large 11-pixel cells by default, or Native 8-pixel cells for solo. Two-player play uses 6-pixel cells for both wells. |
| Start / restart | Restart the cartridge and apply the selected level, game type and speed. |

Settings are saved locally. Each engine keeps its own valid level and game-type choices. Use **Start / restart** to apply changes to the next game. Controller Start or Select opens these M5 settings during Tetris play.

NTSC 60 Hz is the default simulation clock. TetrisGYM's Speed setting provides an explicit PAL option. Simulation timing and LCD refresh are separate; sustained hardware speed is still being tested.

A build with only TetrisGYM supports solo. A build with only Tetris Zero can use its native solo mode as a fallback, with levels 0 through 9. Two-player mode requires the Zero image. A build with neither image keeps the Tetris choice unavailable.

## Buttons

**Tilt move** is the default. Tilt left or right to move the piece. Short-tap the left power button to rotate counterclockwise, or short-tap right to rotate clockwise.

**Tilt rotate** swaps these assignments: short-tap the left and right buttons to move the piece, and lean left or right to rotate. Return to neutral before each additional tilt rotation.

In both presets, tap the front M5 button to move down one row, or hold it to keep soft-dropping. The Down input adapter detects the completed row movement and uses the cartridge's normal collision and gravity behavior.

Hold right for 700 ms to open the M5 menu. While a menu is open, tap right to advance, press front to select, or hold right to exit. The left power button's long hold retains its hardware power function.

Under **Game Settings > Controls**, hold the unit in your preferred neutral position and select **Zero tilt**. Zero tilt saves your current neutral position across restarts. Motion controls require a working motion sensor.

A Bluetooth controller uses these Tetris assignments:

| Input | Action |
| --- | --- |
| Left or Right | Move the piece. |
| Down, tap | Move down one row. |
| Down, hold | Continue soft drop. |
| Up | Hard drop. |
| A or B | Use the cartridge's native rotations. |
| A+B together | Use Hold once until the active piece locks. |
| Start or Select | Open Tetris Game Settings. |

Hold is an M5 frontend extension with its own slot and preview. The first successful Hold stores the current piece, spawns NEXT and runs the cartridge's randomizer for a replacement NEXT. Later Holds exchange the current and held pieces while leaving NEXT unchanged. Another Hold becomes available after the active piece locks. Restart clears the slot.

Large mode draws each occupied cell as an equal 11 by 11 pixel block using colors sampled from the cartridge. Native mode preserves the original 8 by 8 cell artwork. The two-player view fits both wells with equal 6 by 6 cells. Hold and NEXT previews use consistent cell geometry.

The M5 front button keeps its downward-movement assignment. In local two-player mode, the M5 controls Player 1 and Bluetooth controls Player 2. General NES play uses the front button for Start and Select. See [controller setup](CONTROLLERS.md).

H-mode Bluetooth pairing, reconnect after flashing, directional movement and both A/B rotations have been physically confirmed. Captured IINE face-button reports send Space for A and Enter for B; the firmware maps those reports specifically for IINE-keyboard. Manual Up, Down, Hold and board-layout checks, power-cycle coverage and a complete controller playthrough remain pending. A second M5 is unavailable for a physical link playthrough.

## Add the two-player engine

Obtain the Tetris Zero author's patch separately, apply it to a compatible local base ROM, then import the result:

```sh
python tools/apply_ips.py --rom /path/to/base.nes --patch /path/to/zero.ips --output .local/tetris-zero/patched.nes
python tools/import_rom.py --rom .local/tetris-zero/patched.nes --title "Tetris Zero" --output-dir .local/tetris-zero-rom --acknowledge-rights
python tools/build.py
```

The build includes `.local/tetris-zero-rom/` automatically. `--zero-dir` selects another private import folder. This integration targets the mapper 118 v2 patch listed in the [Tetris Zero project entry](https://www.romhacking.net/hacks/1013/). No patch, ROM or cartridge assets are included in this repository.

Choose **Tetris**, then **Game Settings > Players > 2 players**. During local two-player play, the portrait display shows both cartridge playfields. The M5 controls Player 1, and a connected Bluetooth controller controls Player 2. The native cartridge implements the two-player rules.

An experimental two-M5 link sends both players' inputs to identical emulators. Both devices need the same Zero ROM and firmware. Select **Link > Host** on one and **Join** on the other, then mark both ready. Each round reloads the cartridge before running the same launch sequence. Delayed or missing packets stall the shared simulation until both inputs arrive. Native emulator checks compare both games through dropped and reordered packets. A complete physical playthrough with two devices is still pending.

## Experimental two-device competition

Select **Players > Solo** on two M5 devices with the same TetrisGYM cartridge and firmware. Select **Game Settings > Link > Host** on one and **Join** on the other. The host advertises as `M5Tetris-` followed by its identifier. Press the front button on each device to get ready for the shared countdown.

Both devices use TetrisGYM's Seed trainer (type 2) with the same piece seed. The host supplies the starting level and PAL/NTSC choice. Each player controls their own board and sees the other player's score and lines. This mode compares independent games and sends no garbage rows.

Tetris uses separate Bluetooth service identifiers from Flappy Bird. Opening the M5 menu ends the link. Controller pairing and device-to-device play share one Bluetooth stack; starting either disconnects the other. Reconnect through an explicit Host or Join selection.

The protocol has source tests, but a complete two-device physical playthrough is still pending. Shared piece order, launch timing, score display, top-outs, rematches and radio loss need verification on two devices.

## Sound

Tetris includes project-authored cues for movement, rotation, dropping, locking, line clears, level changes and game over. On the StickC Plus, these cues use its GPIO2 passive buzzer alongside a monophonic approximation of an audible NES pulse voice. No imported audio assets are needed for this output.

The global Volume menu offers Off, 20%, 40%, 60%, 80% and Full. It starts at 40%, saves your choice and previews changes. Off stops current playback immediately. A physical listening test is still pending.

## Rights and private files

The project license covers this project's patching tools. Check the base game's rights and the patch author's terms separately before using or sharing their content. The import acknowledgment records your statement and grants no license.

Keep the base ROM, BPS or IPS patch, patched ROM, receipts, generated headers and firmware containing them under ignored `.local/` or another private location. These inputs and outputs are excluded from the public source package. The public tests use synthetic bytes. Read the [rights boundary](RIGHTS.md) and [distribution instructions](DISTRIBUTION.md) before sharing a build.
