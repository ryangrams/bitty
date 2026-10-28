# Mario's clock home

Select Mario to open its portrait clock. Tap left to choose the next game, press M5 for normal Mario play, or hold right for the menu. **Game home** returns from play. The shared **Set clock** menu adjusts the RTC and 12/24-hour preference. Clock mode has been removed from Mario's game settings.

Mario travels through an endless sequence of generated overworld, underground, underwater and castle sections. He enters a pipe, disappears into it, and emerges in the next environment. Each group visits all four themes. Courses include native enemies, pits, blocks, platforms, bridges, firebars, cannons and occasional vine, spring, flag, Toad and Bowser encounters. The clock scene avoids death and has no game-over state.

Coins form HH above MM in clearings, without a colon. The sign uses native 16 by 16 background coin tiles on an equal horizontal and vertical grid. Each digit occupies 48 by 80 pixels, and each centered pair occupies 112 pixels. The camera pauses long enough to read them, then continues. Mario can collect up to three coins per sign. A minute or format change updates the formation. The shared menu offers 12-hour or 24-hour time and an optional weekday/date line at the top or bottom. The rare poison mushroom is omitted because the original supplied ROM lacks Lost Levels artwork.

## Cartridge and engine

The scene accepts the exact standalone 40,976-byte Japan/USA NTSC image with CRC-32 `3337EC46`. It reads CHR, sprite animation tables, metatiles and theme palettes from that local ROM. All original art remains in the user's private input. Unrecognized NES images receive the generic authored NES clock.

This clock has its own small stage simulation and fixed portrait camera. Normal Play reloads the original cartridge in the existing NES emulator, with its original physics, levels, enemies and controller mappings. The clock writes no cartridge data, patches no emulator RAM, and awards no scores or saved progress. The selected scene borrows the existing canvas and owns 704 bytes of host state. It has no extra framebuffer.

The previous landscape clock generator used temporary level-data patches inside the emulator. That generator is retained as legacy source and has portable tests; the current home dispatcher uses `mario_world_clock.h`. The earlier generator's device measurements do not verify this new scene.

## Verification

Source tests use synthetic CHR data to verify native tile decoding, flips, cropping, read-only ownership, minute/format changes, scene bounds and repeated pipe transitions. The private supplied-ROM run exercised 256 seeds and 18,432,000 simulation steps. Every seed visited all four themes and all twelve enemy types, with no emergency safety rebounds. Native-size theme captures and an animated preview are retained privately under `.local/mario-world-clock/`.

The common [game-home policy](GAME_HOMES.md) covers switching, menu behavior and score isolation. The current update still needs its own physical controls and completed-LCD-transfer measurements. Previous hardware results remain in [release status](RELEASE_STATUS.md).
