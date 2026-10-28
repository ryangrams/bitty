---
name: m5stick-development
description: Build and diagnose M5StickC and M5StickC Plus firmware, tiny-screen interfaces, Bluetooth input, piezo audio, sleep and flash storage. Use for actual M5Stick games or apps; verify other M5 models separately.
---

# M5Stick development

Identify the hardware before assigning pins. An ESP32 family name or M5 USB ID does not identify the display, power controller, PSRAM or speaker. Read the project hardware receipt and inspect chip/flash identity. Preserve known-good firmware and settings before partition changes.

For this project, read [hardware and verification](references/hardware.md). For a new board, establish equivalent evidence from its schematic and a minimal probe before reusing those settings.

In Bitty, every game opens on a portrait animated clock home. A short left tap cycles available games, front M5 starts play, and holding right opens the shared menu. Keep Set clock in that menu and return through Game home. Clock simulation must leave scores, rewards and saved progress untouched. Read the repository's [game-home contract](https://github.com/ryangrams/bitty/blob/main/docs/GAME_HOMES.md) and [authoring guide](https://github.com/ryangrams/bitty/blob/main/docs/CREATING_GAMES.md) when adding a Bitty game. This is a Bitty convention; other M5 apps keep their requested interaction.

Separate simulation time, completed video transfers and input sampling. Measure all three on hardware. A 60 Hz loop counter cannot prove 60 displayed frames. Test alternating sprites so frame skipping cannot silently erase blinking actors. Use fixed simulation steps and bounded catch-up. Never speed a game up to improve a rendering metric.

Use bitmap fonts at their native pixel size and integer placement. Cropping preserves pixels; fractional scaling changes stroke widths. Inspect a real-resolution capture. Display transfer buffers remain owned until DMA completes. Confirm RGB565 byte order and display scan direction before changing art colors.

Map every controller button from captured reports. Test A, B, directions and held chords together, press/release edges, reconnection and restart. Keep diagnostic input injection separate from physical-controller evidence. On boards with a power button, verify short-press semantics before assigning it to a held game action.

Validate menus, brightness, piezo output and battery sleep on the actual board. Preserve controller bonds, records and game profiles across routine app updates. A passing build or serial acknowledgement is not evidence of visible display, audible sound or real battery wake.

Keep user ROMs, APKs, extracted art and analysis private. Publish import/build code, authored replacements and license notices. Audit the actual source package and Git history. When adding USB storage, test interrupted writes and reject uploads before erasing data if space is insufficient.
