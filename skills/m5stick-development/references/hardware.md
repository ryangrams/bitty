# Verified M5StickC Plus context

The current device is an ESP32-PICO-D4 with 4 MiB flash and no PSRAM. Its LCD is 135x240 in portrait and 240x135 in landscape. Verify Plus2, original StickC and S3 devices separately.

The existing firmware uses M5Unified and a board-specific power adapter. Read stick_power.h, stick_display.h, piezo_output.h, inactivity.h and their tests before changing those paths. The apparent speaker is driven as a piezo in this build. Keep battery measurements distinct from USB power. Use three verified brightness levels until lower levels are measured.

The CH552 serial bridge has previously stalled when opened directly at a high baud rate or with automatic control-line transitions. The verified diagnostic workflow opens at 9600 with DTR/RTS deasserted, then changes to 115200. Close all handles before expecting a normal physical restart. Never ask for repeated cable resets while another process holds the port.

The project build pins its core and libraries in tools/setup.py and emulator revision in tools/setup_nes.py. Keep generated files under .local. Preserve NVS and controller bonds. Before changing partition sizes, back up the flash and prove the restore procedure.

Hardware timing receipts count completed LCD transfers and elapsed microseconds. Record Bluetooth discovery/connection state, volume, game scenario and simulated versus physical input. Use the existing independent frame-scheduler and DMA ownership tests. Changing the renderer requires a new timing capture only for affected paths.

## StickS3 differences

Verify the separate ESP32-S3-PICO-1-N8R8 target: 8 MiB flash, 8 MiB octal PSRAM, BMI270, ES8311 codec and AW8737 speaker amplifier. The front and right keys are programmable GPIOs. The third key is managed by the PMIC and defaults to reset, double-click power-off and long-hold download. The [M5PM1 v1.9 manual](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf), Button Configuration, exposes single-reset disable at `0x49` bit 0 and double-power-off disable at `0x4a` bit 0. Verify the device/revision before using that map, preserve other bits and read back both writes. Bitty recognizes hardware 5 with firmware 6 or S. Failed writes roll back. Unknown revisions keep a labeled right-tap home fallback.

Poll `BTN_Status` at `0x48` for immediate presses after configuration. Its bit 0 gives the held state and bit 7 latches a press between polls. Do not classify rapid game taps through delayed single/double-click IRQs. Suppress the power-on hold and repeated held actions. Preserve long-hold download recovery. Provide Sleep > Power off when disabling the hardware double-click shutdown gesture. Keep hold-right for the menu.

The [manufacturer’s IMU axis diagram](https://docs.m5stack.com/en/core/StickS3) places Y+ toward portrait screen-right. Horizontal tilt uses negative Y; the original Plus keeps negative X. Apply the same transform to movement samples and zero calibration. Give the corrected S3 axis its own stored neutral keys so earlier X-axis zeros cannot bias it. Confirm the direction physically after loading.

Native USB uses VID `303a` and PID `1001`. The observed serial session works with one 115200 open, DTR high and RTS low. The CH552 low-DTR opening sequence caused repeated S3 resets. Close serial handles before flashing or physical restart. Verify chip, MAC and flash identity before choosing a port when several USB devices are attached.

The Bitty clock command accepts the computer’s local date and time over USB without entering cartridge management. Use the internal ESP32 RTC timer when an external RTC is absent. A full loss of RTC power requires another sync. Browser time must be sampled after opening the connection, so installation and reconnect delays do not make it stale.

The initial physical probe established board detection, full-flash backup verification, application startup and codec/amplifier readiness. Audible output, corrected tilt direction, Bluetooth behavior, completed display timing and battery wake still need their own physical receipts. See the repository’s StickS3 audio document for the current build and acceptance checks.
