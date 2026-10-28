# Bitty menus, battery and startup

The menu uses the pixel mark and dark, violet and amber palette from bitty.games. Every menu, including the skin grid and game picker, has a Bitty header and battery indicator. Ordinary menus fill the display and pause the selected game.

## Battery

The header shows an estimated charge percentage in five-percent steps. A plug means external power is present. On the original StickC and Plus, an amber battery fill also indicates the PMIC’s active charge flag. The pinned StickS3 PMIC driver has no charge-activity implementation, so that target shows external power without claiming active charging. A failed or implausible voltage read shows `--%`. At 15% or below, the battery icon turns orange.

The original Plus uses validated AXP192 status and voltage reads. StickS3 uses the M5PM1 voltage read with an explicit success result. These are voltage estimates, not calibrated remaining-runtime measurements. Menu polling is limited to once per second and does not run in the gameplay rendering path.

## Text and layout

Functional menu text uses the existing Terminus 6 by 12 bitmap cells at native size on the 135 by 240 display. The smaller skin caption and status notices now use that same font. Skin names retain two static lines in their cards. Long runtime notices can scroll. The demo and clock footer is inside the physical portrait crop.

Menu footers name the physical controls: right advances, M5 selects, and holding right exits. Controller directions and A retain their existing menu assignments. The game’s own pixel artwork and HUD fonts remain separate.

## Startup

Five pieces assemble into the Bitty pixel mark. The name follows, with `bitty.games` below. The sequence lasts about 2.4 seconds, including an extra 1.5-second hold on the finished mark. Either front or right button can skip after its first 0.16 seconds. Release the button before playing. The saved boot choice then launches. Ordinary sleep resume retains the game and does not replay the splash.

The animation reuses the existing canvas and asynchronous display buffers. It adds no image asset, framebuffer or font dependency. The Terminus font retains its existing OFL notice.

## Verification

Portable tests cover valid and failed battery readings, percentage boundaries, native crop visibility, animation clipping and buffer guards, opaque menu backgrounds and battery updates confined to the header. The text audit covers every menu page, including Jetpack upgrades and missions. Both board targets compile. Connected-device results are recorded with the private release receipt; a compiled StickS3 build still needs physical checks.

The previous 0.9-second sequence and four menu pages were checked on the original Plus. After the next app-only update, the device measured the extended splash at 2,400 ms across 144 frames. The native Mario Clock settings page was also captured from the running firmware. StickS3 remains compiled and needs a physical check.
