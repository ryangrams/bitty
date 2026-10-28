# Tilt arcade games

Galaga and Arkanoid are native games for the M5StickC Plus's 135 by 240 portrait screen. They use original drawing code and original graphics. Arkanoid uses original generated brick layouts. These are adaptations inspired by the classics, with their own gameplay implementation. They require no additional ROM or asset download.

Choose either game from **Change Game**. Both are also available under **Game at Boot**. Flappy Bird remains the initial boot default.

## Controls

| Input | Galaga | Arkanoid |
| --- | --- | --- |
| Tilt left/right | Move the fighter | Move the paddle |
| Front M5 | Tap or hold to fire; tap after game over to restart | Tap to launch; tap or hold to shoot while Laser is active; tap after game over to restart |
| Tap right side | Pause or resume | Pause or resume |
| Tap left side | Save the current grip as neutral and pause | Save the current grip as neutral and pause |
| Hold right side 700 ms | Open or close the M5 menu | Open or close the M5 menu |
| Controller left/right | Move the fighter | Move the paddle |
| Controller A or B | Fire | Launch or fire |
| Controller Start | Pause or resume | Pause or resume |

A connected controller takes over movement from tilt. The screen stays fixed while the player moves. Side-button gameplay actions use taps. The menu gesture consumes a right-side hold so it cannot also toggle pause. Pause can be resumed with the front button. Buttons held while selecting a game or leaving a menu must be released before firing or launching.

**Game Settings** contains **Restart game**, **Zero tilt**, **Sensitivity**, **Auto fire**, **Difficulty** and **Back**. Sensitivity offers Fine, Normal and Fast. Difficulty offers Easy, Normal and Hard and applies when the game restarts. Auto fire repeats shots in Galaga and while Arkanoid's Laser power-up is active. It does not launch Arkanoid's waiting ball. Settings, tilt zero and each game's high score are saved on the device.

Tilt has a neutral dead zone and light filtering. Movement samples that become stale are ignored. Tilt motion counts as battery activity. Battery sleep retains the game in RAM, following the existing sleep setting. Physical sleep and wake checks remain pending.

## Game rules

Galaga has entrance formations, diving enemies, enemy shots, challenging stages and a boss capture/rescue sequence. Recovering a captured fighter during a dive gives a double fighter. There is no separate power-up button. The [original Bandai Namco description](https://galaga.com/en/history/galaga.php) describes movement, fire and the double-fighter mechanic. This version uses its own flight patterns and artwork.

Arkanoid has durable silver bricks, permanent gold bricks, paddle rebound angles, lives and stage progression. Capsules provide Expand, Laser, Catch, Slow, Multiball or an extra life. Catch temporarily holds the ball on the paddle until the next launch. The [official Arcade Archives listing](https://www.nintendo.com/us/store/products/arcade-archives-arkanoid-switch/) provides a reference to the original game. This version uses its own boards and art.

## Verification

The portable checks cover deterministic gameplay, collisions, lives, score, stage changes, capture/rescue, power-up lifetimes, fast-ball collisions, guarded drawing buffers and input transitions. Both simulations use a fixed 60 Hz tick. On the later Jetpack release, automated input probes measured 500 completed SPI frame intervals in 8,333,429 microseconds for Galaga and 8,333,219 for Arkanoid. Both round to 60.00 FPS. These probes used volume Full with no controller connected. Physical tilt feel, sound and controller play remain unverified.
