# StickS3 audio target

The M5Stack StickS3 is a separate ESP32-S3 device with a 135x240 LCD, 8 MiB flash, 8 MiB octal PSRAM and a built-in 8-ohm, 1-watt speaker. Its ES8311 codec feeds an AW8737 amplifier. This target keeps the existing game engines and screen layouts. Physical sound, controls, Bluetooth, sleep and frame timing on a StickS3 still need verification with that device connected.

Sources: [M5Stack hardware and pin map](https://docs.m5stack.com/en/core/StickS3), [Arduino upload instructions](https://docs.m5stack.com/en/arduino/m5sticks3/program), and the pinned M5Unified 0.2.21 StickS3 speaker configuration and amplifier callback. M5Unified handles the codec and M5PM1 amplifier gate. The application leaves its microphone disabled.

## Build and load

Prepare the pinned dependencies with `python tools/setup.py`. Build the authored starter games and USB cartridge library with:

```sh
python tools/build.py --public --library --board sticks3
```

The S3 image goes into `.local/build-sticks3/`. Its staged source goes into `.local/sketch-sticks3/`. An existing StickC display override is ignored and preserved. The target enables native USB CDC, QIO flash and octal PSRAM. The compiled library layout reserves 2 MiB for the application, starts cartridge storage at `0x210000` and ends below the coredump sector at `0x7f0000`. Settings and controller bonds use the same NVS offsets as the original target.

For a fresh StickS3, enter download mode by holding its reset button for about two seconds until the internal green LED blinks, then release it. Choose its actual USB serial port:

```sh
python tools/flash.py --board sticks3 --port /dev/cu.YOUR_DEVICE
```

The loader checks the selected board and image checksum against the build receipt. ESP32-S3 uses a different bootloader and flash settings from the original ESP32 StickC. Keep the targets separate. Back up an existing installation before changing its partition layout. The public website currently remains an interface preview with firmware downloads disabled; it does not advertise a hardware-verified StickS3 installer.

Local ROMs can be uploaded through the USB library. The private asset import commands also work for S3 builds; replace `--public` with your chosen local inputs. Imported commercial assets and asset-bearing firmware stay private.

## Sound coverage

| Game or screen | Speaker output |
| --- | --- |
| Bitty Flap | Local sample imports when supplied, otherwise authored flap, score and crash cues |
| Mario and general NES cartridges | Mixed NES APU music and effects from the supplied ROM |
| TetrisGYM and Tetris Zero | Mixed NES APU music and effects from the supplied ROM |
| Geometry Dash | Adapted cartridge's NES APU music and effects, with the M5-native display |
| Bitty Swarm, Breaker, Jet, Buddy and Hockey Queen | Distinct authored event profiles for shots, hits, crashes, pickups and wins |
| Menus | Move, selection, back and volume-preview cues |

Native games use synthesized event sounds with square, pulse, triangle and noise voices. Their pitch and rhythm profiles vary by game. Original commercial soundtracks and sound banks are not included. Flappy clock and autoplay modes remain quiet. Menus, game changes, USB management and sleep stop game audio. Resume restarts the speaker after sleep. The S3 I2S sink runs at 24 kHz to match the emulator output; other sample rates use M5Unified's resampler. Audio submission never waits for a full playback queue. Three buffers protect samples borrowed by the speaker task.

Volume choices remain Off, 20%, 40%, 60%, 80% and Full. On StickS3, battery or unknown power caps the speaker master level at 184/255 (about 72%). The affected menu choices show the battery cap. Confirmed external power allows the selected full range. M5Stack recommends staying below 75% on battery to avoid unexpected reboots. Unplugging USB reapplies the cap on the next power poll, within one second. An I2C read failure uses the capped level.

## Physical acceptance checks

1. Start at 40% volume and hear the preview. Verify Off silences cues and ROM music.
2. Play each available game. Check effects, NES music, simultaneous controller input and no audible looping of stale samples after a game switch.
3. Play dense scenes while collecting completed LCD transfers, simulation time, PCM blocks and dropped blocks. Compare against silent output without changing game timing.
4. Open and close menus, switch games, enter and exit USB management, disconnect and reconnect a controller, and sleep then wake on battery. Audio must stop and recover correctly.
5. Verify the battery cap and power detection physically. Check for resets and distortion with representative audio.

The original StickC/Plus GPIO2 buzzer path stays separate. A unit with no fitted speaker cannot reproduce speaker PCM through software alone. The S3 has front M5 (GPIO11), right (GPIO12) and a PMIC-managed left power key. Its default single tap resets the unit. The [M5PM1 v1.9 manual](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf), Button Configuration, documents how to disable that action. Startup checks the PMIC identity and revision, preserves its long-hold download settings, disables single-reset and double-power-off with masked writes, and verifies the values. Supported maps are hardware 5 with firmware 6 or S. Unknown maps or failed writes use right-tap home cycling and show RIGHT NEXT in the footer.

With successful setup, left taps cycle homes, rotate left in the default Tetris layout and fire in Mario. Front M5 and right keep their game controls. The immediate button-status register supplies taps, avoiding delayed single/double-click classification. **Sleep > Power off** replaces the disabled double-click shutdown gesture. A long left hold still enters download mode with the factory timing. Corrected tap behavior remains a physical acceptance check.

The BMI270 reports a different axis orientation from the original Plus. All S3 horizontal movement and zero-calibration paths now use negative-Y acceleration in response to physical feedback about reversed movement. The original Plus keeps its existing negative-X mapping. Confirm the corrected direction after loading. Launching a tilt game captures a fresh neutral position from a short stable sample window. Manual zero remains available in Game Settings.

S3 tilt zeros use separate preference keys, so a saved zero from the previous X-axis mapping cannot bias the corrected controls.
