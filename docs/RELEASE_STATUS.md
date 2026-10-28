# Development preview status

The USB-library branch is ready for source review and automated checks. The public interface preview withholds firmware downloads until hardware installation and transfer checks are complete. The new partition layout, browser installation and runtime transfers still await their first complete hardware check. The connected M5 disappeared from the MacBook's USB device list during its read-only backup. No new firmware was flashed after that disconnect. The previous private game build remains the device's last installed release.

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

Private Jetpack tests retain and collect 10,628 imported formation cells and check the placement changes under sanitizers. The original APK runs offline for reference. Its initial tutorial/revive sequence is not a normal frequency benchmark. Exact vehicle transition choreography, Dragon articulation, finite reel odds, the full gadget catalog and a controlled live course comparison remain open in [the gameplay audit](JETPACK_AUDIT.md).

## Required before calling the USB release hardware verified

1. Reconnect the M5 and finish a verified full-flash recovery backup.
2. Migrate to the new layout, retaining needed settings and controller bonds.
3. Test fresh browser installation, runtime upload, interrupted upload, removal, replacement, restart, boot choice and controller reconnection on the actual board.
4. Record completed LCD transfers and simulation timing for the changed game paths.
5. Check physical controls, piezo output and gameplay presentation. Two-device links require a second unit.
6. Publish linked-library notices, corresponding sources and relinking materials alongside any downloadable firmware. The current Pages preview distributes browser code and no firmware image.

The public marketing site at bitty.games is a separate website task. Its kickoff is prepared. This repository supplies the installer and engineering docs. Studio Upgrade channel creation and a verified Discord invite remain pending; unconfigured links stay hidden. GitHub Sponsors is verified, while hardware affiliate destinations still need verification.
