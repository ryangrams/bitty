# Verified M5StickC Plus context

The current device is an ESP32-PICO-D4 with 4 MiB flash and no PSRAM. Its LCD is 135x240 in portrait and 240x135 in landscape. Verify Plus2, original StickC and S3 devices separately.

The existing firmware uses M5Unified and a board-specific power adapter. Read stick_power.h, stick_display.h, piezo_output.h, inactivity.h and their tests before changing those paths. The apparent speaker is driven as a piezo in this build. Keep battery measurements distinct from USB power. Use three verified brightness levels until lower levels are measured.

The CH552 serial bridge has previously stalled when opened directly at a high baud rate or with automatic control-line transitions. The verified diagnostic workflow opens at 9600 with DTR/RTS deasserted, then changes to 115200. Close all handles before expecting a normal physical restart. Never ask for repeated cable resets while another process holds the port.

The project build pins its core and libraries in tools/setup.py and emulator revision in tools/setup_nes.py. Keep generated files under .local. Preserve NVS and controller bonds. Before changing partition sizes, back up the flash and prove the restore procedure.

Hardware timing receipts count completed LCD transfers and elapsed microseconds. Record Bluetooth discovery/connection state, volume, game scenario and simulated versus physical input. Use the existing independent frame-scheduler and DMA ownership tests. Changing the renderer requires a new timing capture only for affected paths.
