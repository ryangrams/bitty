# Jetpack Joyride for M5

This native adaptation imagines Jetpack Joyride as a small SNES-style game. It fills the M5StickC Plus's 240 by 135 landscape screen with original shaded pixel art and tile-based lab scenery. The camera stays fixed while the lab scrolls past. Each game pixel maps directly to a display pixel.

The [Flipper remake](https://github.com/timstrasser/flipper-jetpack-game) provided a reference for the compact composition and low-resolution character. Halfbrick's game provided the gameplay reference. The M5 version uses new code and artwork. No Flipper source, sprites, original mobile assets or audio are bundled in the public source. A private rebuild can import coin formations and optional runner art from a package supplied by the user. See the [research notes](JETPACK_RESEARCH.md) for sources and the boundary between references and this implementation.

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

Release the action button after closing a menu, waking the screen or changing vehicles before using a fresh tap action. Holding through a crash cannot automatically restart the run. Tilt is unused.

| Ride | Action |
| --- | --- |
| Gravity suit | Tap to reverse gravity, retaining momentum |
| Hog | Tap to jump from the floor, hold for a longer jump |
| Stomper | Tap to jump, hold to extend the boost and soften descent |
| Bird | Tap repeatedly to flap |
| Teleporter | Tap to teleport to the moving target |
| Dragon | Hold to dive, release to rise |
| S.A.M. | Hold to raise the blocking fist, release to lower it |

Vehicle boxes choose among the six standard rides. Collect S, A and M tokens to assemble S.A.M. It faces a missile-blocking sequence and can take multiple hits. The M5 version permits offline activation without a daily lock. Each vehicle acquisition clears hazards and provides a recovery window. Losing a vehicle ejects the runner back into normal flight.

## Rules and settings

Collect coin formations and avoid static and rotating zappers, warned missiles and laser lanes. Scientists walk, flee, react to the exhaust, offer high-fives and tumble when struck by vehicles. They cannot kill the player.

The four power-ups change the run:

- **Shield** absorbs a collision.
- **Boost** gives protected high-speed travel for 100 meters.
- **Cash Dash** clears hazards and supplies coin trails for 60 meters.
- **Rocket Time** slows incoming missiles for seven seconds.

Coin Magnet and Double Coins are also available as bonuses or a starting gadget. Spin tokens feed the end-of-run reward sequence: bonus coins, a second chance, a blast, a headstart for the next run or Double Coins for the next run. Results remain visible before play continues. A crash keeps scrolling while the runner tumbles, bounces and slides. A fresh action during the crash fires one final blast. The body must settle before the run ends. The best-distance record includes distance gained during the crash.

**Game Settings** contains **Restart game**, **Difficulty**, **Starting ride**, **Gadget**, **Headstart** and **Back**. Starting ride offers On foot, each standard ride, S.A.M. or Random. Gadget offers Off, Coin Magnet or Double Coins. Headstart gives 500 meters of protected boost. These choices apply on the next run and persist independently of other games. The initial settings are Normal, On foot, gadget Off and headstart Off.

The default source build includes eight authored formation families. A [private package import](JETPACK_IMPORT.md) can rebuild the supplied APK's original coin layouts. The package inspected during development yielded 147 layouts, including 28 verified classic-base formations, plus separate cash and onboarding layouts. Runtime collection masks retain every point of a dense formation. Event layouts are retained privately without enabling the original events. The default import keeps this project's sprites.

Volume and battery sleep use the shared device settings. The GPIO2 buzzer plays short event cues. Original mobile music and audio are excluded.

## Verification

The simulation advances at a fixed 60 ticks per second. Drawing uses the existing shared framebuffer and DMA display queue. Only the selected native game's state is allocated. Gameplay and drawing allocate no memory per frame.

Normal flight follows the gravity, thrust, drag and running-speed behavior recovered from a privately supplied Android reference. The vertical travel range is fitted to this screen. Normal running speed rises from about 67.08 to 184 screen pixels per second over 2,250 meters. The Gravity Suit retains momentum while a tap reverses its gravity, following the recovered gravity and drag model. Easy and Hard change horizontal pacing. The other rides share this course-speed curve and use M5 control and collision adaptations informed by recovered configuration. Their complete phone state machines have not been reproduced. The obstacle layouts and safety spacing are project-created. See [the reference boundary and coordinate fit](JETPACK_RESEARCH.md#provenance).

Portable tests cover deterministic movement, acceleration, speed limits, each ride and power-up, SAM fist overlap, vehicle body collision, laser separation, immediate Cash Dash trails, scientist interactions, every spin reward, crash settling, fresh input and guarded display buffers. A protected pool stress check runs 54,000 ticks across nine ride IDs. Separate natural-run fuzz checks cover death and retry. The state occupies 1,796 bytes and allocates no memory per frame.

The source-only importer checks use synthetic packages. A separate synthetic runtime suite tests all 512 cells in a dense formation, overlapping groups, selection weights, cash-only selection, Double Coins, reset, optional runner rendering and fallback. The privately supplied package was also checked by rendering all 147 formations and collecting all 10,628 cells without truncation. These private inputs remain outside the source ZIP.

The expanded firmware was flashed to the M5StickC Plus and its flash hash verified. Nine hardware probes started on foot, in each of the seven rides, and with headstart plus magnet. Each measured 500 completed SPI frame intervals. The 4,500 intervals averaged **59.9963 FPS**, with individual probes from **59.9825 to 60.0004 FPS**. The simulation reported 60 Hz in each probe. Volume was Full. The early probes included Bluetooth scanning. No controller was available to reconnect during this test.

Pause froze the simulation counter, the menu rotated to portrait and returned to landscape, and automatic testing preserved all native-game high scores. Diagnostic input and loadout overrides were cleared. The device was left at the Jetpack start screen with ordinary settings restored and the serial connection closed.

These are completed LCD transfer measurements. They do not measure the panel's scan phase. Physical inspection of tearing, sound, controller feel for the new rides and a full playthrough remains open.

Independent motion checks compare the fixed-point implementation with a double-precision reference calculation, including sustained flight, release, midair input changes and Gravity Suit reversals. The normal model reaches the ceiling in 57 ticks and the floor in 70 at starting speed. At full speed those checks take 40 and 48 ticks. These are analytical reference comparisons, separate from hardware or Android runtime measurements.

The independent fairness check compares over 100,000 capsule collisions against a separate geometric oracle, checks 1,024 generated zappers for ceiling and floor threats, and searches for normal-flight routes through 18 sampled four-zapper sequences. Each found input sequence is replayed through the actual game and checked against independent geometry without invulnerability. These checks cover normal-flight zappers; they do not prove every generated course or missile and vehicle combination is reachable. The search uses optimization with sanitizers in the source test runner to keep its runtime practical.
