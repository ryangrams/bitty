# Bluetooth controllers

Selecting **Controller > Pair controller** starts a Bluetooth Low Energy HID scan. A successful connection shows **Controller ready**. **Disconnect** closes the link; select Pair controller again to reconnect. **Forget pairing** removes the last selected controller's bond when its identity is known in the current session.

The Controller page stays visible during pairing so you can read its status.

Controller mode, Flappy Bird co-op and the Tetris competition link share one Bluetooth stack. Starting controller pairing ends a device-to-device link, and starting either game link disconnects the controller.

## Battery sleep and reconnect

**Sleep** defaults to 30 seconds without input while running on battery. The menu also offers 1, 2, 5 or 10 minutes and Off. USB power keeps the device awake. During sleep, the screen and Bluetooth turn off while game state stays in RAM. Wake the M5 with its front or right side button, then release it before playing. Connecting USB also wakes it through periodic power checks.

The M5 tries to resume a controller retained from the current powered session, including after an ordinary disconnect before sleep. An explicit **Disconnect** stays off. Wake the controller too if needed. **Controller > Pair controller** remains available for an explicit scan and device selection. Bluetooth shutdown and retained-peer reconnect passed a hardware check within two seconds. Battery sleep, button wake and USB wake still need physical checks.

## IINE Gamebrick Mini

Use the H keyboard profile first:

1. Move the back switch to **H**, identified by a yellow light.
2. Use its default keyboard profile, **IINE-keyboard**. The manufacturer lists arrow keys for the D-pad and A and B for the buttons. The firmware also accepts the Space and Enter face-button reports captured from the tested controller.
3. Hold the controller's **Pairing** button for three seconds until the indicator flashes.
4. On the M5, open **Controller > Pair controller** and wait for the status.

Pairing plus A changes H mode to the **IINE-mouse** profile. Select the keyboard profile for game controls. These mode names and button instructions come from the [official IINE guide for L1161 and L1162](https://iine.store/pages/gamebrick-mini-retro-controller-guide-manual).

The manufacturer's C mode, identified by a red light, advertises **Xbox Wireless Controller** by default. Pairing plus A changes it to **Wireless Controller**. These profiles may work if they expose BLE HID input reports understood by this build. Their advertised names alone do not establish compatibility. The official guide does not identify the Bluetooth transport used by each profile. H-mode pairing and directional movement have been physically confirmed; C-mode profiles still need testing.

## Controls

| Context | Input | Action |
| --- | --- | --- |
| Flappy Bird | A | Flap, start or replay. |
| Flappy Bird | B | Open the menu, or advance while open. |
| Menus | A | Select highlighted item. |
| Menus | Down or Right | Next item. |
| Menus | Up or Left | Previous item. |
| NES | D-pad | NES directions. |
| NES | A and B | NES A and B. In Super Mario Bros., these are jump and run. |
| NES | Controller Start and Select | NES Start and Select. |
| Mario player selector | Controller D-pad or Select; either M5 side button | Change 1 PLAYER / 2 PLAYERS. |
| Mario player selector | Controller A, B or Start; front M5 | Confirm the selected mode. |
| Mario player selector | Controller A+B together | Send native A+Start to continue at the first stage of the last world reached. |
| Mario, no controller | Hold front M5 | Move with run held. |
| Mario, no controller | Double-tap front M5 | Reverse direction. Holding the second press continues movement. |
| Mario, no controller | Right side button, short tap | Jump. |
| Mario, no controller | Left side button, short tap | Re-press B to shoot when fire-powered. |
| NES | Front M5 button, short release | Start. |
| NES | Front M5 button, hold at least 700 ms | Send Select once. |
| Tetris | Controller Left or Right | Move the piece. |
| Tetris | Controller Down, tap | Move down one row. |
| Tetris | Controller Down, hold | Continue soft drop. |
| Tetris | Controller Up | Hard drop. |
| Tetris | Controller A or B | Use the cartridge's native rotations. |
| Tetris | Controller A+B together | Use the dedicated Hold slot once until the piece locks. |
| Tetris | Controller Start or Select | Open Tetris Game Settings. |
| Tetris, Tilt move | Tilt left or right | Move the piece. |
| Tetris, Tilt move | Left or right side button, short tap | Rotate counterclockwise or clockwise. |
| Tetris, Tilt rotate | Left or right side button, short tap | Move left or right. |
| Tetris, Tilt rotate | Lean left or right | Rotate once, then return to neutral before rotating again. |
| Tetris, either preset | Front M5 button, tap | Move down one row. |
| Tetris, either preset | Front M5 button, hold | Continue the cartridge's soft drop. |
| Any game | Controller Menu | Open the M5 menu. |
| Any game | Right side M5 button | Hold for 700 ms to open the menu. Tap to advance while open; hold again to exit. |

During controller-driven Mario gameplay, ordinary NES directions and A/B combinations pass through, including a held direction with A or B. Automatic run is limited to the built-in movement controls. Mario waits for an explicit player-mode choice, and A+B continue requests during Game Over wait for the title screen. In two-player Mario, the shared controls reach both NES ports for alternating Mario and Luigi turns. Tetris uses Up for hard drop and A+B for Hold. Other direction and rotation combinations remain available.

Tetris Hold is an M5 frontend extension. The first Hold stores the current piece and consumes NEXT, then advances the cartridge's randomizer for the new NEXT. Later Holds exchange the current and held pieces without changing NEXT. Locking the active piece makes Hold available again, and restart clears the slot. See [Hold and board settings](TETRIS.md#buttons).

In general NES play, the front M5 button supplies Start and Select for controllers such as the IINE Gamebrick Mini that lack those buttons. Release it before 700 ms for Start, or hold it for Select once. In Tetris, the front button always controls downward movement, including while a controller is connected.

Tetris defaults to **Tilt move**. **Tilt rotate** swaps the roles of tilt and the two side buttons. Calibrate the motion sensor under **Game Settings > Controls > Zero tilt** while holding the unit in your preferred neutral position. Zero tilt saves the neutral position across restarts. The left power button retains its hardware long-hold function. Tetris Zero's native two-player mode uses the M5 controls for Player 1 and a connected controller for Player 2. See [Tetris settings and competition](TETRIS.md).

Dedicated controller Start and Select use their NES assignments in Mario and open Game Settings in Tetris. Controller Menu opens the M5 menu. The generic gamepad mapping uses HID button usages 1 and 2 for A and B, and 9 and 10 for Select and Start. Different physical layouts may need a mapping change.

Generic keyboard reports map A, B, arrow keys, Enter to Start, Backspace to Select, and Escape to Menu. For **IINE-keyboard** only, Space maps to game A and Enter maps to game B. Letter A and B remain accepted in that profile. This handles the face-button reports captured from the tested IINE unit. Other keys have no assigned game action.

## Compatibility and status

### Additional controller test targets

The Gamebrick Mini in H mode remains the physically tested controller on the original Plus. New controller models need their own pairing, report-map, combined-button, reconnection and gameplay tests before appearing as compatible purchase recommendations.

| Candidate | Why test it | Evidence and current limit |
| --- | --- | --- |
| Xbox Wireless Controller, Series X/S model 1914 | A full-size option with dedicated Start/Menu and View buttons | [Microsoft documents Bluetooth LE](https://news.xbox.com/en-us/2020/03/16/xbox-series-x-controller/). [Bluepad32's device list](https://github.com/ricardoquesada/bluepad32/blob/main/docs/supported_gamepads.md) identifies model 1914 and firmware 5.15 or newer as BLE. Bitty pairing and mapping are untested. |
| 8BitDo Ultimate 2C Wireless, Windows/Android version | A second full-size test target with an Android Bluetooth mode | [8BitDo specifies Bluetooth for Android](https://www.8bitdo.com/ultimate-2c-wireless-controller/). Confirm its actual BLE transport and HID reports before claiming Bitty support. Similar Ultimate product names refer to different hardware. |

StickS3 supports Bluetooth LE only. Espressif documents that [Bluetooth Classic is unavailable on ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-guides/bluetooth.html). The original ESP32 has a different radio, while Bitty's current controller stack also uses BLE exclusively. A Bluetooth label or Android compatibility claim alone does not establish compatibility. Several existing 8BitDo models use Classic HID, as listed in the Bluepad32 protocol table. Keep untested controllers out of the supported-device list.

This build supports the BLE HID service (`0x1812`), reads its Report Map and Report Reference descriptors, and subscribes to input notifications. It accepts bounded keyboard and gamepad reports, report IDs, button arrays, directional bits, hats and absolute X/Y axes. Keyboard boot mode is a fallback when available. Proprietary initialization sequences, sparse array maps, mouse control and Bluetooth Classic HID profiles are unsupported.

Connections request encryption with bonding and no man-in-the-middle requirement. A controller that requires entering or confirming a PIN gets a visible status; this build has no PIN-entry or confirmation screen. Disconnects, invalid reports and receive overflow clear affected held inputs.

The IINE has reached **Controller ready** and reconnected after flashing. Physical checks confirm Tetris movement and both A/B rotations. In Mario, A jumps, B+Right runs, and A still jumps while B+Right stays held. Captured A and B presses sent Space and Enter; the IINE-only mapping handles those reports and passes host tests.

Manual Tetris Up, Down and Hold checks, Mario title shortcuts, power-cycle coverage and complete controller playthroughs remain pending. C-mode profiles and other controllers still need physical testing.
