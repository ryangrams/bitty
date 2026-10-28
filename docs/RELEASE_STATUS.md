# Development preview status

The source and public interface preview are available. Firmware downloads remain disabled until the new partition layout, browser installation and runtime transfers pass hardware checks. On October 28, a full 4 MiB recovery backup was completed and verified against the original Plus. A private Jetpack placement build was then flashed as an app-only update within its existing 3 MiB application partition. This retained its partition table, settings, controller bonds and game records. It does not establish that the new USB-library layout works on hardware.

## Completed in source

- Atomic cartridge storage, checksums, twelve records, removal with reusable flash allocations, boot selection and immediate launch.
- Browser USB transfer with request IDs, bounded input, local ROM preflight and rights acknowledgement. Files travel directly to the device.
- A two-column device game picker with native bitmap text, game icons and separate generic NES cartridge entries.
- Public builds with authored starter art and private ROM/APK discovery disabled, a locked installer bundle, provenance and separate flash parts.
- Mario camera tracking from stable RAM coordinates. Small, standing big and crouching big Mario have distinct anchors. Animation and invincibility blinking no longer choose the tracking target.
- Imported Jetpack formation pitch, vertical placement, isolated spacing, distance gates, obstacle profiles and smooth rotation.
- Reusable M5Stick development, game-fidelity and content-to-GitHub skills, including Codex and Claude installation instructions.

## Verification

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
