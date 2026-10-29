# Bitty Buddy

Run a Little Buddy through three lanes, collect pocket change, and dodge people much bigger than you. The rounded character has hands and feet, a sweatshirt, and a colorful LED mask. Rolling gets you under open legs. Jumping gets you over low obstacles.

Ryan and Roman developed the concept together. This first playable version uses original C++ game logic and native pixel artwork. It needs no imported game files.

## Play

Choose **Bitty Buddy** from Change Game or cycle to its clock home. Press the front M5 button to start.

| Action | Stick | Controller |
| --- | --- | --- |
| Change lane | Tilt left, neutral, or right | Tap D-pad Left or Right to move one lane |
| Jump | Front M5 button | A |
| Roll | Short right-side button press | B |
| Pause | Short left-side button press, which also saves neutral tilt | Start |
| System menu | Hold right | Menu |
| Retry or continue after a celebration | Front M5 button | A or B |

A jump lasts about 0.9 seconds and a roll about 0.6 seconds. Release and press again for the next action. A short jump buffer helps near landing. Rolling in the air dives toward a slide; jumping can interrupt a roll. The right-side roll fires on release so a long hold can open the shared menu. Release held controls after starting, returning from a menu, waking, or reconnecting a controller.

People standing straight require a lane change. People with spread legs have a downward arrow marking the roll gap. Low obstacles can be jumped. A collision ends the run unless a hoverboard absorbs it.

## Five locations

Each level lasts ten minutes. Obstacle density and speed increase as you advance. Every generated row leaves a reachable open lane. Six-row sections alternate slaloms, hurdles, low passages, crowds, mixed rhythms, coin bonuses and recovery stretches. Speed and density ramp during the first three minutes of each location. The small-screen ceiling preserves reaction time. A section progress strip shows the next change.

| Location | Native artwork |
| --- | --- |
| Pocket Mall | Shop windows, bags, strollers, and shoppers |
| After School | Lockers, classroom doors, and book stacks |
| Pocket Park | Trees, fences, benches, and people |
| Lava Laundry | A volcano, lava, glowing rocks, and people |
| Noodle Night | Lanterns, noodle bowls, and people |

At the end of a level, the Little Buddy celebrates. Wait three seconds, then press an action button to continue. After the final location, another action returns to a fresh run.

## Pocket finds

- **Coins:** Safe ground trails guide movement. Riskier arcs above hurdles and trails beneath open legs reward jumps and rolls. Bonus sections add longer coin trails.
- **Hoverboard:** A shield lasting up to ten seconds. It absorbs one collision, followed by a short grace period.
- **Rubber snake:** Clears the next ten obstacles. It clears approaching obstacles first, then spends any remaining charges on newly arriving ones.
- **Electronic parts:** Every three parts change the current LED mask.
- **Shirts:** Change the current sweatshirt color.
- **Magnet:** Coins visibly travel toward the runner for eight seconds. Every coin credits once, and ordinary coin spawning continues.
- **Score boost:** Doubles the current score multiplier for ten seconds.
- **Balloon:** An eight-second flight over people and obstacles. Coins rise into an airborne trail, the hoverboard timer pauses, and a grace period protects the landing.
- **Pocket prize:** Adds fifteen coins and grants a magnet or hoverboard.

Each run has three goals: collect coins, jump hurdles and roll through gaps. Completing the set adds ten bonus coins, grants a hoverboard, and raises the run multiplier. The multiplier caps at eight before the temporary score boost. Goals restart with increased trick targets. Successful jump and roll dodges build a trick chain and extra points. Goals, rewards and appearance finds belong to the current run; the device saves the best score.

The character faces down the course during running, jumping, rolling and flight. The LED face appears during celebrations and the crash intermission. Functional score, section and goal labels use the native menu bitmap font.

Game Settings selects the starting location, shirt, LED mask, difficulty, and tilt sensitivity. It also provides tilt calibration and restart. Starting choices and the best score are saved on the device. Coins, parts, and pickup appearance changes belong to the current run. The clock home keeps its own animation state and awards no gameplay progress.

## Native browser playtest

The private browser playtest runs the same engine and renderer through a small host process. It supports keyboard and touch controls, starting-location selection, appearance choices, pause, restart, and the animated clock home.

Build the host and start the server from the repository root:

```sh
mkdir -p .local/buddy-preview
c++ -std=c++11 -O2 tools/buddy_host.cpp -o .local/buddy-preview/buddy-host
python3 tools/buddy_preview_server.py --bind 127.0.0.1 --port 4186
```

For remote play, bind the host's verified ZeroTier address explicitly. The browser receives native 135 by 240 RGB565 frames. The simulation runs at 60 fixed steps per second, with bounded catch-up. Network conditions affect input latency and displayed frame delivery. The preview currently has no sound.

Capture themed art and character poses with:

```sh
c++ -std=c++11 -O2 tools/buddy_preview.cpp -o .local/buddy-preview/buddy-capture
mkdir -p .local/buddy-preview/captures
.local/buddy-preview/buddy-capture --out .local/buddy-preview/captures
```

These captures use authored scene arrangements rendered by the game's pixel renderer. The interactive playtest uses the live game simulation. All capture and build output stays local.

## Verification

The portable engine tests cover collision rules, action timing and rearming, hoverboard protection, ten snake clears, collectible changes, all five ten-minute levels at three difficulties, fair course generation, and deterministic random input. Renderer tests cover all five locations, six character poses, clipping guards, deterministic drawing, and the clock's reserved footer. Shared tests cover menus, game switching, boot selection, clock allocations, and input isolation.

The source suite passes with AddressSanitizer and UndefinedBehaviorSanitizer. The original Plus public USB-library build compiles within its existing 2 MiB application partition. The gameplay audit adds tests for magnet animation and single credit, flight and coin collection, paused shields, goal rewards, trick scoring, course scheduling, buffered actions and rear-facing poses. This candidate has not been flashed. Physical tilt, controller feel, sound, sleep/wake, and completed LCD timing still need a device playtest.

The Mac Studio preview runs through a dedicated user LaunchAgent. Its working directory stays in the user home because launchd could not start it with a working directory and log paths on the external volume. Python reads the project and launches the native host from Shades. The service is bound only to the selected ZeroTier interface.
