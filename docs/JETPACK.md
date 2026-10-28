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
| Action before a Final Spin | Spend one token and start the reels |
| Left side tap or controller Down before spinning | Cash remaining tokens at 100 coins each |
| Fresh action after the reels stop | Continue from the result |

Release the action button after closing a menu, waking the screen or changing vehicles before using a fresh tap action. Holding through a crash cannot automatically restart the run. Tilt is unused.

| Ride | Action |
| --- | --- |
| Gravity suit | Tap to reverse gravity, retaining momentum |
| Hog | Tap to jump from the floor, hold for a longer jump |
| Stomper | Tap to jump, hold to extend the boost and soften descent |
| Bird | Tap repeatedly to flap |
| Teleporter | Tap to begin teleporting to the moving target |
| Dragon | Hold to dive, release to rise |
| S.A.M. | Hold to raise the blocking fist, release to lower it |

Hog and Stomper taps can be buffered just before landing. Profit Bird adds each accepted flap to its existing momentum and requires ten released ticks before another flap. Teleporter pauses its marker during departure, moves to it after the departure phase, then plays its arrival phase.

Vehicle boxes choose among the six standard rides. Collect S, A and M tokens to assemble S.A.M. It faces a missile-blocking sequence and can take multiple hits. The M5 version permits offline activation without a daily lock. Each vehicle acquisition clears hazards and provides a recovery window. Losing a vehicle ejects the runner back into normal flight.

## Rules and settings

Collect coin formations and avoid static and rotating zappers, warned missiles and laser lanes. Scientists walk, flee, react to the exhaust, offer high-fives and tumble when struck by vehicles. They cannot kill the player.

The four power-ups change the run:

- **Shield** absorbs one collision at baseline and up to five after upgrades.
- **Boost** targets 100 meters of high-speed travel at baseline.
- **Cash Dash** clears hazards, supplies coin trails for 60 meters and ends with a shower target you must catch.
- **Rocket Time** eases the world and normal flight into slow motion, schedules missile waves with possible gem trails, then offers distinct vehicle choices. Its baseline configured duration is seven seconds, with entry and exit phases around it.

Coin Magnet and Double Coins are available as practice gadgets. Magnet coins visibly travel toward the runner and credit on contact. Boost keeps ordinary coin formations spawning and uses swept contact checks during fast travel. Cash Dash shower coins fall and bounce after a short collection lock; Magnet can attract them after the lock. The shared effect pool holds at most 48 value particles, so one shower particle can carry several coins.

Spin tokens feed the end-of-run reward sequence: bonus coins, a second chance, one of three blast strengths, three more tokens, a headstart for the next run or Double Coins for the next run. Three animated reels stop in sequence before showing the result. Results remain visible before play continues. Spending another token requires a fresh action. A crash keeps scrolling while the runner tumbles, bounces and slides. A fresh action during the crash fires one final blast. The body and horizontal travel must settle before the run ends. The best-distance record includes distance gained during the crash.

**Game Settings** contains **Restart game**, **Difficulty**, **Missions**, **Power-up upgrades**, **Practice** and **Back**. Missions shows three active goals, their progress and rank stars. Power-up upgrades shows the saved coin bank and the price of each next level. Shield gains hits, Boost and Cash Dash gain distance, and Rocket Time gains duration. Purchases apply when the next run starts.

Coins, mission progress, rank rewards, SAM letters and earned next-run items persist on the device. Coins are checkpointed when opening the menu or sleeping, and the run settles once after its bonuses finish. Leaving or restarting a run cashes remaining tokens at 100 coins each. A second chance continues the same run. The saved profile includes a version and checksum.

Practice contains the free starting ride, gadget and 500m headstart options. Enable Practice to use those loadouts. Starting rides, headstart and the Boost, Cash Dash or Rocket Time starting bonuses use mutually compatible selections. Magnet, Double Coins and Shield can accompany a starting ride. Practice excludes saved earnings, missions, SAM progress, reward inventory and high scores. Normal runs begin on foot and use earned items. The initial settings are Normal difficulty and Practice Off.

See the [vehicle and bonus audit](JETPACK_AUDIT.md) for current fixes and remaining gaps. See [progression rules](JETPACK_PROGRESSION.md) for pickup scheduling, native bonus values, upgrades, missions and the explicit M5 adaptations.

The default source build includes eight authored formation families. A [private package import](JETPACK_IMPORT.md) can rebuild the supplied APK's original coin layouts. The package inspected during development yielded 147 layouts, including 28 verified classic-base formations, plus separate cash and onboarding layouts. Runtime collection masks retain every point of a dense formation. Event layouts are retained privately without enabling the original events. The default import keeps this project's sprites.

Volume and battery sleep use the shared device settings. The GPIO2 buzzer plays short event cues. Original mobile music and audio are excluded.

## Verification

The simulation advances at a fixed 60 ticks per second. Drawing uses the existing shared framebuffer and DMA display queue. Only the selected native game's state is allocated. Gameplay and drawing allocate no memory per frame.

Normal flight follows the gravity, thrust, drag and running-speed behavior recovered from a privately supplied Android reference. The vertical travel range is fitted to this screen. Normal running speed rises from about 67.08 to 184 screen pixels per second over 2,250 meters. The Gravity Suit retains momentum while a tap reverses its gravity, following the recovered gravity and drag model. Easy and Hard change horizontal pacing. The rides use their own recovered or configured horizontal profiles, with a final vehicle cap of 1000 native units per second after difficulty scaling. Hog, Stomper, Profit Bird and Teleporter controls now use the audited behavior described in [the current audit](JETPACK_AUDIT.md). Dragon articulation, complete vehicle transitions and some original wrapper behavior remain incomplete. Collision proportions use M5 fits. The course scheduler follows recovered content weights, meter-based pickup periods, anti-repeat vehicle rules and token guarantees. Hazard geometry and safety gaps are fitted to this screen. See [the reference boundary and coordinate fit](JETPACK_RESEARCH.md#provenance).

Portable tests cover deterministic movement, acceleration, speed limits, each ride and power-up, SAM fist overlap, vehicle body collision, laser separation, Cash Dash trails and collectible payloads, scientist interactions, every spin reward, crash settling, fresh input and guarded display buffers. A protected pool stress check runs 54,000 ticks across nine ride IDs. Separate natural-run fuzz checks cover death and retry. The vehicle and bonus changes add fixed-capacity state, including the shared 48-particle coin pool. They allocate no memory per frame. The game state occupies 3,832 bytes on the ESP32 and 3,840 bytes in the host test build.

The source-only importer checks use synthetic packages. A separate synthetic runtime suite tests all 512 cells in a dense formation, overlapping groups, selection weights, cash-only selection, Double Coins, reset, optional runner rendering and fallback. The privately supplied package was also checked by rendering all 147 formations and collecting all 10,628 cells without truncation. These private inputs remain outside the source ZIP.

The vehicle and bonus firmware was flashed to the M5StickC Plus and the programmer verified its data hash. Twelve probes each measured 500 completed SPI frame intervals while gameplay remained active. They covered on-foot flight, all seven rides, headstart with Magnet, a repeatedly replenished dense magnet shower, maximum-tier Cash Dash and maximum-tier Rocket Time. The 6,000 intervals averaged **59.9667 FPS**, with individual probes from **59.6573 to 59.9969 FPS**. The dense shower was the slowest case. Volume was Full, Bluetooth discovery was enabled and the device retained its saved Easy difficulty. No controller connected during these probes. Diagnostic protection suppressed hit resolution so crashes could not turn the timing test into a static game-over screen.

Rocket Time showed its slowdown and emitted three waves containing sixteen missiles during its timing window, with zero dropped shots. The reel animation started and reached its settled state. Pause froze simulation, and the menu rotated to portrait and returned to landscape. Automatic tests preserved the profile checksum and all native-game high scores. Diagnostic input, protection and loadout overrides were cleared. The device was left at the Jetpack start screen with ordinary settings restored and the serial connection closed.

These measurements cover completed LCD transfers. Panel scan phase, physical inspection of tearing, audible sound, controller feel for the new rides and a full playthrough remain open. The source ZIP also passed its complete tests after extraction: 131 Python checks, portable C++ sanitizer suites, and synthetic valid and invalid imported Rocket courses. The real private pack separately passed the core and bonus suites, including all 10,628 coin cells.

Independent motion checks compare the fixed-point implementation with a double-precision reference calculation, including sustained flight, release, midair input changes and Gravity Suit reversals. The normal model reaches the ceiling in 57 ticks and the floor in 70 at starting speed. At full speed those checks take 40 and 48 ticks. These are analytical reference comparisons, separate from hardware or Android runtime measurements.

The independent fairness check compares over 100,000 capsule collisions against a separate geometric oracle, checks 1,024 generated zappers for ceiling and floor threats, and searches for normal-flight routes through 18 sampled four-zapper sequences. Each found input sequence is replayed through the actual game and checked against independent geometry without invulnerability. These checks cover normal-flight zappers; they do not prove every generated course or missile and vehicle combination is reachable. The search uses optimization with sanitizers in the source test runner to keep its runtime practical.
