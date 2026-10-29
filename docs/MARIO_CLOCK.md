# Mario's clock home

Mario's home runs the supplied NTSC game in the NES emulator. Its own engine handles jumps, collisions, enemies, moving orange platforms, swimming, pipe entry and death. The portrait display copies a 135-column crop from the full native 256 by 240 frame. It keeps the full vertical world and uses no stretching or vertical camera movement.

Choose Mario to open the clock, press M5 for normal play, tap left to change games, or hold right for the shared menu. Manual play retains the Mario/Luigi selector and the existing controller and internal-button assignments.

## Repeatable courses

Four authored seven-page courses cycle through overworld, underground, underwater and castle environments. Each has two clearings for the time, plus native obstacles and encounters. The first two courses finish with an enterable pipe. The other two finish with a flag sequence. The clock repeats the courses indefinitely.

The director sends ordinary NES button input. It brakes before a clearing, swims with separated button presses, reacts to approaching enemies, and aligns Mario over the exit pipe. Coin digits show hours above minutes, without a colon. They use native 16 by 16 tiles on a square grid. A small horizontal camera adjustment keeps the entire formation visible while Mario's original movement inertia settles. Mario can pass through some of the displayed coins.

A death plays briefly and restarts the same course. Two consecutive deaths advance to the next course. Bounded boot, movement and course timeouts provide recovery. Flag celebrations have a bounded dwell. These protections belong to the home director; manual Mario retains the cartridge's rules.

Time formatting and the optional weekday/date line come from the shared Set clock menu. The clock updates the displayed digits without changing the course's physics. Its simulation uses the existing 60 Hz home scheduler and requests every NES video frame. Completed LCD throughput still needs a measurement on each board.

## Local cartridge and ownership

The adapter accepts the standalone Japan/USA NTSC image: 40,976 bytes, CRC-32 `3337EC46`. Other cartridges receive the generic NES clock. Users supply their own lawful local file. No Nintendo graphics, program code or original level lists are included in the public source.

The authored courses replace level-data bytes in two private cached program windows. The source ROM stays read-only. Switching to manual play unloads the home engine and reloads the user's original cartridge, releasing the patches. The clock is silent and writes no scores, coins, missions or saved gameplay progress.

The director lives in [mario_attract.h](../firmware/flappy/mario_attract.h). Its course recipes live in [mario_clock.h](../firmware/flappy/mario_clock.h). The separate native sprite simulation in `mario_world_clock.h` is legacy source and is no longer dispatched by the firmware's home screen.

## Verification

Portable tests cover authored course bounds, one NES frame request per normal step, stale RAM immediately after CPU reset, death retry, repeated-failure advance, clipped native-pixel composition and read-only cartridge ownership. NES core checks cover CPU, PPU, APU, timing, input, program caching and video paths. Private supplied-ROM traces separately check actual native area types, moving-platform actors, pipe entries, goals, recovery counts and readable clock captures.

Source tests and host traces establish their named scenarios. Physical tilt, controls, sound, battery wake and completed LCD timing remain separate device checks. See [release status](RELEASE_STATUS.md) for hardware evidence.
