# Mario Clock

Choose **Mario > Game Settings > Mode** to switch between **Play** and **Mario Clock**. The change starts a fresh game or course and returns to the game. **Set clock** uses the shared hardware RTC and 12/24-hour preference. **New game / course** starts again. The mode is saved across power cycles. Choosing Mario as Game at Boot also uses that saved mode.

Clock mode runs the supplied Japan/USA NTSC cartridge in the NES emulator. Mario runs and jumps through generated ground courses, reaches the flag and castle, then starts another course. Each course has two or three clearings with the current time built from native eight-pixel floor tiles. Mario pauses for three seconds at each clearing. The sculptures scroll with the world and update when the minute changes. They are decorative blocks with no collision. Course obstacles use the cartridge's normal collision and physics.

The generator varies course length, scenery, pipe and block placement, and obstacle height. It keeps continuous ground and limits obstacles to one or two blocks high. Original enemy placements are disabled for these generated courses. This first clock generator makes ground courses. It does not generate the original game's underground, water, castle or enemy encounters.

Mario Clock is quiet. It leaves the saved global volume setting alone. Normal play restores cartridge sound. The existing battery inactivity setting still applies, including the default 30-second sleep. USB power keeps the device awake. Hold right to open the normal M5 menu in either mode. Select Play to return to manual Mario.

## Cartridge and memory

This feature requires the exact standalone 40,976-byte Japan/USA image with CRC-32 `3337EC46` and a display that supports the native 240 by 135 view. The mode remains unavailable for an absent, unknown or multicart image. Supply your own ROM through the private import process described in [NES cartridges](NES.md). The public project includes no Nintendo ROM, artwork or audio.

Generated objects replace level data in two owned, temporary 8 KiB PRG cache windows. The original imported ROM remains unchanged. Returning to Play reloads the original cartridge. The patch API accepts bounded writes only to owned mapper-0 cache buffers. The clock state is allocated only while clock mode is active. Allocation failure returns to Game Settings and restores Play on the next update.

The time uses an eight-pixel floor tile sampled from the running cartridge. Its composition happens in a scratch strip within the unused end of the existing canvas, before the normal dirty-row comparison. Scrolling, changed digits and removing the clock therefore use the same display-damage and DMA ownership rules as the rest of Mario. It adds no framebuffer. The simulation and video divisor remain NTSC 60 Hz and every-frame composition. Source frames and completed LCD transfers remain separate measurements.

## Verification

Portable tests cover 1,000 generator seeds, ordered and bounded level streams, obstacle clearances, deterministic repeats, minute and 12-hour changes, clipped tile composition, buffer guards, scrolling erasure, menu transitions and read-only ROM ownership. The NES cache tests reject unsupported mappers, unowned banks and out-of-bounds writes.

A private host test uses the supplied NTSC cartridge and the same generator and controller. It runs 128 consecutive generated courses through one emulator instance, using native controller input for movement. Every course must reach the native flag/castle sequence, advance the level and stop at every clock clearing. Private captures inspect the resulting terrain and clock at native resolution. The ROM, captures and analysis remain in `.local/mario-clock/`.

The updated Plus firmware was flashed as an app-only update after a fresh partition-table and NVS backup. The written hash verified. On the original Plus, two probes each completed 500 SPI frame intervals: Mario Clock took 8.332089 seconds and manual Mario took 8.332015 seconds, both approximately 60.009 completed SPI frame boundaries per second. NTSC simulation stayed at 60 Hz with video divisor one. BLE discovery was active and no controller connected. Clock mode had 29,124 bytes free heap. Opening Pair controller while in clock mode retained the course seed and generated PRG cache. The records and Jetpack profile matched their pre-update values. Serial was closed with Mario Clock running. The device framebuffer captures include a clock clearing and its settings menu. These checks measure completed DMA transfers, independently of panel scanout and physical controller feel. The public StickS3 target compiles and still needs its own hardware checks.

Level encoding and RAM addresses were checked against [the NES disassembly](https://www.6502disassembly.com/nes-smb/SuperMarioBros.html) and [Matthew Earl's level-extraction analysis](https://matthewearl.github.io/2018/06/28/smb-level-extractor/). The generator and its level streams are authored for this project.
