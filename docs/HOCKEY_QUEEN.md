# Hockey Queen

Hockey Queen is a native 135 by 240 adaptation of Ryan's air-hockey game. Solo play uses one stick against the CPU. Two-stick play remains available. Each player sees their own paddle at the bottom. Tilt left or right to move it; the puck moves through collisions. Power-ups activate when collected. The first player to seven wins.

The Queen gets power-ups by default. Game Settings can give them to both players or turn them off. Pink lasers, Rainbow Beam, Mega Paddle, Shrink Ray, Rainbow Wall, Triple Puck, Slime Time, Backwards Day, Butterflies, Rocket Puck, Tiny Goal and Freeze Ray have compact implementations. Extra pucks only score for their owner. A single active power per player replaces the original game's stacking effects. Banana Peel and map-specific special powers are omitted from this adaptation.

## Solo match

Press M5 from the clock home. Tap right to choose Queen or Peasant, then press M5 to enter a match on the configured random or fixed map. The CPU defaults to Medium. Game Settings offers Easy, Medium, Difficult and Evil. Reaction delay, movement speed and aim error vary by skill. It predicts side-wall rebounds, while bumpers, powers and later hits can defeat its prediction. It uses the same movement and collision rules as the human player.

Tap right to pause, M5 to resume, and M5 after the result for a new match. Tilt is automatically zeroed when play starts. Game Settings > Players switches between solo and two sticks.

## Two-stick match

1. Install the same Hockey Queen firmware on both compatible sticks. Select Hockey Queen to open its animated clock home.
2. Set **Game Settings > Players** to **Two sticks** on each unit. On the first stick, press M5 to host as the Queen. On the second, choose **Game Settings > Two-stick link > Join as Peasant**. Alternatively, press M5 from its clock, then tap right to switch from hosting to joining.
3. When connected, hold each stick in a comfortable neutral position and press M5 on each. This zeros that stick's tilt and marks its player ready. Both players must be ready for the shared countdown.
4. After the match, press M5 on each stick for a rematch. A random map is selected again from the new match seed. The map stays fixed between goals.

Tap left during play to save a new neutral tilt. Hold right for the shared system menu. Opening that menu ends the current link; reconnect through Two-stick link to play again. Game home returns to the clock. Tilt uses the calibrated horizontal axis on both screens, with the Queen's world input reversed to match her rotated view.

Bluetooth is used for the connection between the two sticks. Hockey Queen uses the sticks' tilt sensors and does not require a Bluetooth controller. Starting a match releases an existing controller connection while preserving its saved bond. A Bluetooth controller and a two-stick link are separate radio modes in the current firmware.

## Maps and colors

The default map is Random. Game Settings also offers Rainbow Court, Unicorn Road, Narwhal Ice, Jellyfish Reef, Butterfly Meadow, Candy Castle, Potato Patch and Throne Room. Settings apply to the next match. There is no map picker between starting and playing or between goals.

The host supplies the map and color settings to the guest. Map colors, Night, Neon and Pastel are available. The miniature maps retain selected features: Unicorn Road's boost lane, warned narwhal bumpers, drifting jellyfish, meadow gusts, candy bumpers, potato mounds and mud, and throne pillars. Decorations and character faces are drawn directly at native pixel sizes. The interface reuses Bitty's bitmap menu font and shared brightness, volume, sleep and clock controls.

The portrait clock home runs a separate automatic match with the time painted on the rink. It starts no radio connection and saves no match progress or scores. Left cycles games, M5 starts manual play and holding right opens the menu.

## Simulation and link

The game advances at 60 fixed updates per second with four collision substeps per update. The host owns the puck physics, pickups, powers, scores and match transitions. The guest sends horizontal input and draws received authoritative state with a 33 ms interpolation delay. It does not run a competing puck simulation.

Hockey uses its own BLE service UUID with the existing host/join lobby, session identifier, round, ready and countdown protocol. Motion and board packets each fit the default 20-byte application payload. A guest commits a frame only when both parts have the same sequence. Inputs and snapshots reject old sequences, invalid coordinates, unsupported fields and the wrong session, round or direction of travel. Sequence and millisecond comparisons handle wrap. The guest expands the 16-bit wire counter into the full world timer so moving rink features keep the host's phase across long matches.

The host pauses gameplay after 250 ms of stale guest input, or when the guest reports that its complete view is stale. The guest holds its last complete view and shows Link wait after 250 ms without a complete snapshot. Both resume from current authoritative state when delivery recovers. Three seconds without a valid protocol packet ends the session. A physical disconnect ends it immediately. Joining currently chooses the nearby matching host with the strongest signal. Keep the intended pair close together. The BLE connection inherits the existing unencrypted local-link behavior.

## Source and checks

The reference is the local Hockey Queen project at source commit `b671107a8434ba44cbf14d53b327e06ca3e249fa`. Rules were read from its Queen Rules data, base powers and eight map definitions. This adaptation supplies fresh native C++ code and compact geometric art. It includes no Godot engine, story assets, soundtrack, imported sprite sheets or commercial game files.

Run `python tools/test.py` for source-only game, renderer, clock, menu and protocol checks. The Hockey tests cover all maps and palettes, twelve powers, scoring, deterministic long play, pixel-buffer guards, paired snapshots, reordering, packet loss, stale input, session checks, rematches and clock wrap. The preview-capable game test can write host-rendered PPM frames when given an output prefix.

Run `python tools/preview_hockey.py` with the project requirements installed to recreate native PNG frames, an eight-map sheet and a comparison of both sticks with the clock home. Output defaults to `.local/hockey-queen/preview/`. These are host-rendered images. Power effects in the comparison frames are deliberately activated by the test harness for inspection.

Two-stick discovery, real tilt direction, perceived latency, completed display transfers, audio and reconnect after sleep still require testing on the physical pair. A host simulation and a successful firmware compile do not establish those hardware results.
