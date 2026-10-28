# Development preview status

The source and public interface preview are available. Firmware downloads remain disabled until the new partition layout, browser installation and runtime transfers pass hardware checks. On October 28, a full 4 MiB recovery backup was completed and verified against the original Plus. A private Jetpack placement build was then flashed as an app-only update within its existing 3 MiB application partition. This retained its partition table, settings, controller bonds and game records. It does not establish that the new USB-library layout works on hardware.

## Completed in source

- Atomic cartridge storage, checksums, twelve records, removal with reusable flash allocations, boot selection and immediate launch.
- Browser USB transfer with request IDs, bounded input, local ROM preflight and rights acknowledgement. Files travel directly to the device.
- A two-column device game picker with native bitmap text, game icons and separate generic NES cartridge entries.
- Public builds with authored starter art and private ROM/APK discovery disabled, a locked installer bundle, provenance and separate flash parts.
- Mario camera tracking from stable RAM coordinates. Small, standing big and crouching big Mario have distinct anchors. Animation and invincibility blinking no longer choose the tracking target.
- Mario now waits at an explicit one-player/two-player selector. Side taps choose and M5 confirms. With no controller, hold M5 to run, double-tap to reverse, tap right to jump and tap left to fire when powered. A paired controller keeps NES controls and M5 Start/Select. Native two-player turns use both NES input ports. The private NTSC probe verifies selection, reversal, a 49-source-pixel tap jump, fire-button edges and Luigi's input after Mario loses a life. Physical checks for these controls are pending.
- Every game opens on a portrait clock home. Left cycles games, M5 plays, and Game home returns from play. Shared clock settings replace per-game Clock mode entries. [The scene guide](GAME_HOMES.md) covers coin, tetromino, enemy, pipe and brick clocks.
- [Hockey Queen](HOCKEY_QUEEN.md) adds native two-stick air hockey, horizontal tilt, random maps, twelve compact powers, eight rinks, four color choices and a separate clock home. The host owns match physics; paired BLE snapshots and stale-input pauses keep the guest on the same state. Portable game, menu and packet-loss tests pass. Original Plus and StickS3 builds compile. Two-device radio, controls, latency, sound and completed LCD measurements remain pending. This candidate has not been flashed.
- Mario's new endless clock reads native art from the supplied NTSC ROM, uses pipe transitions through four environments and includes enemies and collectible coin time. Private tests run 256 seeds and 18,432,000 simulation steps without emergency safety rebounds. [The clock notes](MARIO_CLOCK.md) distinguish this scene from the previously flashed generator.
- Clock time now uses stacked hours over minutes without colons across every scene. Mario uses native square coin cells, and Bitty Jet and Hockey Queen also use two rows. Shared settings explicitly select 12-hour or 24-hour time, plus None, Top or Bottom for a weekday/date line. Bottom occupies the footer so it leaves players and paddles visible. The date menu writes the RTC; portable checks cover leap years, midnight, 100 years of weekdays and uptime wrap. These changes have not been flashed.
- Bitty menu branding, native 6 by 12 text, a battery estimate and an animated startup mark with the website address. The splash now lasts 2.4 seconds.
- Imported Jetpack formation pitch, vertical placement, isolated spacing, distance gates, obstacle profiles and smooth rotation.
- Reusable M5Stick development, game-fidelity and content-to-GitHub skills, including Codex and Claude installation instructions.

## Verification

The new universal clock-home update passes the source suite with AddressSanitizer and UndefinedBehaviorSanitizer. A separate Linux GCC run passes the new scenes, shared runtime and menu checks. The private Plus image fits its existing 3 MiB app partition; authored public Plus and StickS3 builds fit their 2 MiB app partitions. Native-size private previews cover every scene. This update is ready for device testing and has not been flashed or measured on hardware. Earlier hardware results below apply to their named builds.

The earlier 0.9-second Bitty UI build was flashed to the original Plus with its partition table and records retained. Four menu framebuffer captures covered Home, game selection, skins and Controller. Its AXP192 reported external power, active charging and roughly 4.11 V, yielding a 100% voltage estimate. The next app-only update added the longer splash and Mario Clock, with its written hash verified. The Plus measured the splash at 2,400 ms and both clock and manual Mario at approximately 60.009 completed SPI frame boundaries per second over separate 500-interval probes. BLE discovery was active, no controller connected, and clock mode retained 29,124 bytes free heap. Controller pairing from clock mode retained its generated course cache. Records and profile matched before and after. The serial handle was closed with Mario Clock running. Panel scanout and physical controller feel remain separate checks. StickS3 still needs its own hardware measurements.

The [StickS3 audio target](STICKS3_AUDIO.md) adds a separate ESP32-S3 build, native codec output, NES PCM music and effects, authored Flappy fallback sounds, menu volume previews and a battery volume cap. Both hardware families compile. Portable tests cover audio buffer ownership, backpressure, target isolation, flash receipt validation and power detection. These are software checks. Audible output and simultaneous audio, video and Bluetooth performance still need a connected StickS3.

The source suite covers Python import and publication checks, portable C++ state machines, storage interruption tests, renderer bounds and asynchronous display ownership. AddressSanitizer and UndefinedBehaviorSanitizer are enabled for portable runtime checks. The browser protocol has a mocked Web Serial test for fragmented replies, delayed duplicates, CRCs, upload acknowledgements and receive limits. These tests do not substitute for a physical USB session.

The private NTSC Mario reference ran 6,000 frames through full and cropped video paths with identical CPU RAM and displayed pixels. The run contained 5,152 moving-platform actor-frames. A separate ROM probe checked standing, crouching and small-player sprite bounds. Platform simulation stayed unchanged. This establishes emulator and crop-path agreement, while controller feel and camera presentation still need a physical playthrough.

Private Jetpack tests retain and collect 10,628 imported formation cells and check the placement changes under sanitizers. The placement build also passed four device probes covering on-foot play, dense magnet coins, Dragon and Rocket Time. Across 2,000 completed LCD intervals, throughput averaged 59.9944 frames per second with Full volume and BLE discovery active. No controller connected. Records and profile stayed unchanged through the probes. Physical sound and controller feel remain unverified for these changes. The original APK runs offline for reference. Its initial tutorial/revive sequence is not a normal frequency benchmark. Exact vehicle transition choreography, Dragon articulation, finite reel odds, the full gadget catalog and a controlled live course comparison remain open in [the gameplay audit](JETPACK_AUDIT.md).

## Required before calling the USB release hardware verified

1. Retain the verified full-flash recovery backup before migration.
2. Migrate to the new layout, retaining needed settings and controller bonds.
3. Test fresh browser installation, runtime upload, interrupted upload, removal, replacement, restart, boot choice and controller reconnection on the actual board.
4. Record completed LCD transfers and simulation timing for runtime-loaded game paths. The app-only Jetpack timing probes are complete.
5. Check physical controls, piezo output and gameplay presentation. Two-device links require a second unit.
6. Publish linked-library notices, corresponding sources and relinking materials alongside any downloadable firmware. The current Pages preview distributes browser code and no firmware image.

The public marketing site at [bitty.games](https://bitty.games) is maintained separately. This repository supplies the installer and engineering docs. The marketing site maintains its community, Sponsors and hardware links, with compatibility labels and paid-link disclosures.
