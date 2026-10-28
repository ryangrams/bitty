# Game clocks

Select a game and its clock starts in portrait. Tap the left side button to move to the next available game. Press the front M5 button to play. StickS3 startup configures its power key for taps and disables accidental single-tap reset and double-tap power-off. If its PMIC revision cannot be configured, the footer shows RIGHT NEXT as a fallback. Hold right for the menu on either model. Choose **Game home** in the menu to return to the selected game's clock. **Game at Boot** chooses which clock appears after startup.

A paired controller can use Left for the next home, A or Start to play, and B or Menu for the menu. Gameplay keeps each game's existing controller assignments. The original Plus power key keeps its hardware long-hold function.

**Set clock** is in the main menu for every game. **Time format** selects **12-hour** or **24-hour**. **Day / date** cycles through **None**, **Top** and **Bottom** for a shared weekday and date line, such as `WED OCT 28 2026`. Bottom uses the footer in place of the repeated button hint, leaving the native game area visible. **Set date** adjusts the year, month and day. The USB loader can set the date and local time from the computer. Boards with an available RTC keep that clock; other boards may need another sync after full power loss. Display preferences survive restart. All clock scenes show hours above minutes without a colon.

The clocks are quiet and do not award records, coins, missions or other saved progress. Battery inactivity sleep still applies. The default is 30 seconds; USB power keeps the clock awake.

## The scenes

| Game | Clock behavior |
| --- | --- |
| Mario | An endless generated course visits overworld, underground, water and castle sections through animated pipes. Native 16 by 16 coin tiles form HH above MM on a square grid in clearings. Mario can collect some coins, jump obstacles and stomp enemies. |
| Flappy Bird | Double-size source pixels, an immortal bird and no score. Pipes gather into two rows of time digits and return to pipes every 800 steps, about 4.5 appearances per minute at 60 Hz. |
| Tetris | Genuine connected tetrominoes fall into a 10-column well. Bright digit cells and dim supporting cells make HH over MM legible. After a readable hold, the bot clears lines and continues playing. An 18-second cycle rebuilds the board about 3.3 times per minute. Both Tetris engines use this scene. |
| Geometry Dash | Double-size forms travel through a fixed portrait view. Object digits scroll through the course. Each ten-second cycle eases into quarter-speed world motion over one second, holds for four, and eases back over one second. Drawing continues on every update. |
| Galaga | Enemies fly into digit formations, dive and return. The ship shoots them apart with real projectiles, then the next formation arrives. |
| Arkanoid | Randomized boards include durable colored time bricks. The paddle aims at bricks through ball collisions. A visible safety rail keeps the scene running. A natural clear or minute change wipes the board and builds another. |
| Hockey Queen | Two tiny paddles play across themed rinks while the time appears in the center markings. This separate home scene needs no second stick or radio. |
| Bitty Jet | Coins form a clock in the lab, hold briefly for reading, then scatter and attract toward the runner with an animated magnet. Staff, hazards and vehicles continue around it. |

Flappy gameplay digits now follow the selected skin's palette. Midnight uses subdued colors. The authored Toast and Past Due skins use their own inks; imported color skins draw from their bird palette.

## Mario artwork

Mario's clock requires the supplied standalone Japan/USA NTSC image: 40,976 bytes, CRC-32 `3337EC46`. Its native CHR, animation, metatile and palette data are read directly from that local ROM. The project distributes no Nintendo graphics. Other NES cartridges receive an authored generic NES clock.

The scene includes native original-game enemies and terrain objects. The rare Lost Levels poison mushroom is omitted because that artwork is absent from the supplied original-game ROM. See [Mario's scene](MARIO_CLOCK.md) for scope and verification.

## Rules for new games

Every new game implements the same home contract. Keep clock selection and menu handling in `game_home.h` and the firmware input layer. A scene supplies `reset(seed)`, `step(ClockTime)` at 60 Hz, and a const `draw(pixels, ClockTime)` for a 135 by 240 RGB565 canvas. It borrows the existing display canvas. It must keep records, profiles, rewards and gameplay state outside its simulation. Add its dispatcher cases to `clock_runtime.h` and its source paths to `PUBLIC_FILES.txt`.

Animated scenes target the 135 by 240 display. An 80 by 160 StickC receives a native bitmap time-and-controls fallback. That smaller-panel fallback still needs hardware verification.

The selected scene owns only its small state allocation. Game switching drains display transfers before reusing the canvas. Starting play releases the clock before allocating the game engine. Ordinary NES play retains its NTSC scheduler and every-frame Mario composition. Clock slow motion affects only the clock scene's world movement.

Portable checks cover all nine internal game identifiers, missing-ROM fallback, minute/format changes, clipping guards, deterministic scenes, repeated switching and allocation failure. Individual scene tests cover collisions, formation cycles, real tetromino geometry and motion continuity. ROM-bearing previews and private reference probes stay under `.local/`. These checks do not establish physical button behavior, LCD scanout or device throughput for this update. Hardware receipts are recorded separately in [release status](RELEASE_STATUS.md).

The build keeps `-O2` and places Xtensa literal constants beside their functions with `-mtext-section-literals` to keep the enlarged native game source within instruction range. See [GCC's Xtensa option reference](https://gcc.gnu.org/onlinedocs/gcc/Xtensa-Options.html). This affects code placement; simulation timing remains unchanged.
