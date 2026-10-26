# Two-device race

Use two compatible ESP32 M5 devices running the same firmware. Choose **Co-op host** on one and **Co-op join** on the other. The host advertises a name such as `M5Flap-123ABC`. Joining searches for 3.5 seconds and selects the matching host with the strongest received signal. Keep the intended pair close together. If several hosts are nearby, check the selected name, leave the lobby if needed, and retry.

Each player presses the front action button to get ready. Both ready signals are required before the shared countdown starts. After a crash, wait for both players to finish, then press action on both devices for another round. A surviving player keeps flying while the other watches. The menu ends the connection, and another race requires an explicit host or join selection.

Both players fly the same course at the same time. Each controls their own bird and sees the other bird on screen. Birds do not collide with each other. The host chooses the course seed and the shared color assignment. The opposing bird uses the renderer's 180-degree hue shift, with inversion for sprites close to gray. Skins are local display choices. Co-op uses the Original rule set, and race scores stay out of the saved single-player high score.

## Connection behavior

Bluetooth starts when you choose host or join. Leaving co-op releases the Bluetooth stack. A disconnected session requires another user selection before advertising or scanning again.

The connection carries a temporary session identifier, course seed, round, readiness, countdown, color, world tick, bird position, rotation, animation frame, life state and score. It has no internet service, account, phone companion or upload step. The BLE connection is unencrypted. A nearby compatible device can attempt to join an advertising host, so verify the peer in the lobby before pressing ready.

The host accepts one connected peer. Packet fields, version, size, session and round are checked before they change game state. Packets fit the default BLE application payload: the largest packet is 19 bytes. A fixed queue holds up to 12 received events, and excess events are dropped. Control messages repeat so a dropped queue entry can be recovered during a healthy connection.

During play, each device sends its bird state at up to 20 updates per second. The other bird is drawn with a 50 ms interpolation delay when two recent samples are available. Interpolation spans are limited to 100 ms. Longer gaps hold the latest position, and the bird disappears once its sample is 500 ms old. Three seconds without a valid protocol packet ends the session. A physical BLE disconnect ends it immediately.

## Timing and course contract

The host sends the seed and a countdown repeatedly. The guest acknowledges the start, and the host cancels a countdown that receives no acknowledgement. Both devices launch from the shared round seed. The guest refines its local start deadline from repeated countdown messages and follows the host's world tick during play.

The course is indexed by seed and world tick in `Game`. Pipe positions and gaps are deterministic for that pair of values, including when a player's bird has already crashed. The display clocks can differ by a few frames because BLE delivery and local rendering take time. This mode provides a shared-course race with a visible opponent. It has no server validation of human inputs or scores, rollback, anti-cheat or tournament ranking.

`BleLink` keeps the transport separate from the portable protocol. The device layer uses [NimBLE-Arduino 2.3.6](https://github.com/h2zero/NimBLE-Arduino/tree/2.3.6). Its connection and callback APIs are checked against the pinned [client](https://github.com/h2zero/NimBLE-Arduino/blob/2.3.6/examples/NimBLE_Client/NimBLE_Client.ino) and [server](https://github.com/h2zero/NimBLE-Arduino/blob/2.3.6/examples/NimBLE_Server/NimBLE_Server.ino) examples.

The sketch follows this integration contract:

1. Call `beginHost(now, sessionId, seed, hue)` or `beginJoin(now)` after a menu choice. Use fresh random, nonzero session and seed values for hosting.
2. Call `update(now)` from the main loop. Scan callbacks and incoming BLE data enter a bounded queue. Game state changes on the main task. The lobby's connection and service discovery may briefly block.
3. Call `requestReady(now)` on an action press in the lobby or after the local bird reaches Game Over. During a race, ordinary action presses go to the local bird.
4. Consume each `StartInfo`, reset the game with its seed, and enable `Game::enableSharedMap(seed)`. Read `session().startDeadlineMs()` for the latest countdown deadline. Launch once the phase becomes `Playing`.
5. Use `worldTick(now)` to synchronize the map. Clamp the target to at least the game's current world tick so a timing correction cannot rewind the course. Before a live physics step, sync to the target tick before that step. Continue syncing the map after local death.
6. Feed `setLocal(PlayerSnapshot)` with the actual game tick, bird state and score. Draw `ghost(now)` when `visible` is true. Use `session().role()` to apply a consistent host/guest color assignment on both screens.
7. Call `stop()` when leaving co-op. Exclude co-op from persistent single-player scores.

## Verification

The portable test suite covers packet round trips, every truncated packet length, invalid values, version and session rejection, readiness, start acknowledgements, deterministic round seeds, restart coordination, interpolation, stale ghosts, timeouts and unsigned clock wrap. It also simulates delayed delivery with every seventh outgoing packet dropped and sends 10,000 generated inputs through the decoder. It runs with AddressSanitizer and UndefinedBehaviorSanitizer. The BLE wrapper also has an ESP32 compile check with the pinned library.

The temporary Bluetooth compile probe and its build cache were removed after verification. Recreate that check by compiling the full firmware with `python tools/build.py`, which exercises both host and join paths.

Two-device radio operation still needs a physical test. Confirm discovery and selected names, both ready buttons, simultaneous launch, matching pipes, each player's independent input, ghost colors, crashes, rematches, menu exit, distance-related loss and reconnection. Timing claims beyond the protocol's configured delays require measurement on two devices.
