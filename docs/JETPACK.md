# Jetpack Joyride for M5

This native adaptation imagines Jetpack Joyride as a small SNES-style game. It fills the M5StickC Plus's 240 by 135 landscape screen with original shaded pixel art and tile-based lab scenery. The camera stays fixed while the lab scrolls past. Each game pixel maps directly to a display pixel.

The [Flipper remake](https://github.com/timstrasser/flipper-jetpack-game) provided a reference for the compact composition and low-resolution character. Halfbrick's game provided the gameplay reference. The M5 version uses new code and artwork. No Flipper source, sprites, original mobile assets or audio are bundled. See the [research notes](JETPACK_RESEARCH.md) for sources and the boundary between references and this implementation.

Choose **Jetpack Joyride** under **Change Game** to play immediately. It is also available under **Game at Boot**. Flappy Bird remains the initial boot default. This game needs no ROM or separate asset import.

## Controls

| Input | Action |
| --- | --- |
| Front M5, held | Fire the jetpack and climb |
| Front M5, released | Fall and run along the floor |
| Controller A or B, held | Fire the jetpack and climb |
| Tap either side button | Pause or resume |
| Controller Start | Pause or resume |
| Hold right side 700 ms | Open or close the M5 menu |
| Front M5 while paused | Resume |
| Fresh action on the title or game-over screen | Start or retry |

Release the action button after closing a menu or waking the screen before using it again. Holding the button through a crash cannot automatically restart the game. The Gravity Suit changes the action to **tap to flip gravity** and displays that hint while equipped. The armored jetpack keeps the usual hold-to-fly control. Tilt is unused in this game.

## Rules and settings

Collect coin trails and avoid electrical zappers and missiles. Missile warnings appear before the launch. Running speed increases with distance. Vehicle pickups absorb a collision, then return you to the normal jetpack with a short recovery window. This version includes the armored jetpack and Gravity Suit. Its course, physics and art are project-created adaptations.

**Game Settings** contains **Restart game**, **Difficulty** and **Back**. Difficulty offers Easy, Normal and Hard, applies on the next run and is saved separately from Galaga and Arkanoid. The device saves the best distance. Volume and battery sleep use the shared device settings. The buzzer uses short event cues for thrust, coins, vehicle pickups and crashes.

## Verification

The simulation advances at a fixed 60 ticks per second. Drawing uses the existing shared framebuffer and DMA display queue. Only the selected native game's state is allocated. Gameplay and drawing allocate no memory per frame.

Normal flight follows the gravity, thrust, drag and running-speed behavior recovered from a privately supplied Android reference. The vertical travel range is fitted to this screen. Normal running speed rises from about 67.08 to 184 screen pixels per second over 2,250 meters. The Gravity Suit retains momentum while a tap reverses its gravity, following the recovered gravity and drag model. Easy and Hard change horizontal pacing. The obstacle layouts and safety spacing are project-created. See [the reference boundary and coordinate fit](JETPACK_RESEARCH.md#provenance).

Portable tests cover deterministic movement, acceleration, speed limits, coins, obstacle geometry, missile warnings, vehicle ejection, retry input and guarded display buffers. The source ZIP also passes these checks without imported assets. The firmware was uploaded to the M5StickC Plus and its flash hash verified.

The device measured 500 completed SPI frame intervals in 8,333,343 microseconds during Jetpack play, or 60.00 FPS. With the IINE controller connected in H mode and volume Full, a second 500-interval probe measured 8,332,242 microseconds, or 60.01 FPS. The simulation remained on its fixed 60 Hz clock. Automated checks also verified pause, portrait menu rotation, landscape return and preservation of high scores. The device was left at the Jetpack start screen with the controller connected and diagnostic inputs disabled.

These measurements cover completed LCD transfers. Physical inspection of tearing, controller feel, sound and a full playthrough remains open.

Independent motion checks compare the fixed-point implementation with a double-precision reference calculation, including sustained flight, release, midair input changes and Gravity Suit reversals. The normal model reaches the ceiling in 57 ticks and the floor in 70 at starting speed. At full speed those checks take 40 and 48 ticks. These are analytical reference comparisons, separate from hardware or Android runtime measurements.

The independent fairness check compares over 100,000 capsule collisions against a separate geometric oracle, checks 1,024 generated zappers for ceiling and floor threats, and searches for normal-flight routes through 18 sampled four-zapper sequences. Each found input sequence is replayed through the actual game and checked against independent geometry without invulnerability. These checks cover normal-flight zappers; they do not prove every generated course or missile and vehicle combination is reachable. The search uses optimization with sanitizers in the source test runner to keep its runtime practical.
