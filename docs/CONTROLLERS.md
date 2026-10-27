# Bluetooth controllers

Pair a controller through **Controller > Pair controller**. The host scans for Bluetooth Low Energy HID devices only after you select that action. A successful connection shows **Controller ready**. **Disconnect** closes the link; select Pair controller again to reconnect. **Forget pairing** removes the last selected controller's bond when its identity is known in the current session.

Controller mode and the two-device co-op link share one Bluetooth stack. Starting controller pairing ends co-op, and starting co-op disconnects the controller.

## IINE Gamebrick Mini

Use the H keyboard profile first:

1. Move the back switch to **H**, identified by a yellow light.
2. Use its default keyboard profile, **IINE-keyboard**. The manufacturer maps the D-pad to arrow keys and its buttons to A and B.
3. Hold the controller's **Pairing** button for three seconds until the indicator flashes.
4. On the M5, open **Controller > Pair controller** and wait for the status.

Pairing plus A changes H mode to the **IINE-mouse** profile. Select the keyboard profile for game controls. These mode names and button instructions come from the [official IINE guide for L1161 and L1162](https://iine.store/pages/gamebrick-mini-retro-controller-guide-manual).

The manufacturer's C mode, identified by a red light, advertises **Xbox Wireless Controller** by default. Pairing plus A changes it to **Wireless Controller**. These profiles may work if they expose BLE HID input reports understood by this build. Their advertised names alone do not establish compatibility. The official guide does not identify the Bluetooth transport used by each profile, and physical Gamebrick Mini pairing has not yet been verified.

## Controls

| Context | Input | Action |
| --- | --- | --- |
| Flappy Bird | A | Flap, start or replay. |
| Flappy Bird | B | Menu button: tap to open or advance; hold to exit. |
| Menus | A | Select highlighted item. |
| Menus | Down or Right | Next item. |
| Menus | Up or Left | Previous item. |
| NES | D-pad | NES directions. |
| NES | A and B | NES A and B. In Super Mario Bros., these are jump and run. |
| NES | Controller Start and Select | NES Start and Select. |
| NES | Front M5 button, short release | Start. |
| NES | Front M5 button, hold at least 700 ms | Send Select once. |
| Any game | Controller Menu | Open the M5 menu. |
| Any game | Right side M5 button | Tap to open or advance the menu; hold to exit. |

All D-pad and A/B combinations pass through to the NES game. The front M5 button supplies Start and Select for controllers such as the IINE Gamebrick Mini that lack those buttons. Release it before 700 ms (0.7 seconds) for Start, or hold it for Select. A long hold sends Select once.

Dedicated controller Start, Select and Menu buttons keep their assignments. The generic gamepad mapping uses HID button usages 1 and 2 for A and B, and 9 and 10 for Select and Start. Different physical layouts may need a mapping change.

Keyboard reports map A, B, arrow keys, Enter to Start, Backspace to Select, and Escape to Menu. Other keys have no assigned game action.

## Compatibility and status

This build supports the BLE HID service (`0x1812`), reads its Report Map and Report Reference descriptors, and subscribes to input notifications. It accepts bounded keyboard and gamepad reports, report IDs, button arrays, directional bits, hats and absolute X/Y axes. Keyboard boot mode is a fallback when available. Proprietary initialization sequences, sparse array maps, mouse control and Bluetooth Classic HID profiles are unsupported.

Connections request encryption with bonding and no man-in-the-middle requirement. A controller that requires entering or confirming a PIN gets a visible status; this build has no PIN-entry or confirmation screen. Disconnects, invalid reports and receive overflow clear affected held inputs. Physical pairing, reconnect behavior and button timing still require a controller test.
