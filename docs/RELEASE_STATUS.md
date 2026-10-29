# Development preview status

The current StickS3 gameplay update is installed and its application readback matches the build. The original Plus candidate compiles, but its update and two-stick radio tests are pending a reliable USB connection. Public firmware downloads remain disabled while the browser installer and cartridge-storage layout await hardware acceptance.

## Current changes

- The device remembers its last selected game and cartridge. The visible Game at Boot option is removed.
- The game picker uses colored cards and native bitmap text. The startup animation places a large Bitty mark above “bitty” and a smaller “.games”, with a 2.4-second hold.
- Authored game names are Bitty Flap, Bitty Swarm, Bitty Breaker, Bitty Jet, Bitty Buddy and Hockey Queen. User-loaded cartridge titles remain identifiable. See [naming](NAMING.md).
- StickS3 horizontal tilt uses negative Y in response to feedback about reversed movement. The original Plus retains negative X. Tilt games take a fresh neutral sample window on launch. Physical confirmation of the corrected S3 direction remains required.
- Geometry Dash keeps the front M5 button as its action button with or without a connected controller. Silent startup screens advance in bounded batches that stop before gameplay. Game physics retain NTSC timing.
- Hockey Queen offers solo Queen or Peasant play, four CPU skills with Medium as the default, and the existing two-stick mode. Human tilt input has increased gain. [Hockey notes](HOCKEY_QUEEN.md) describe the controls and link rules.
- Native games have distinct pitch and rhythm profiles. StickS3 cues use square, pulse, triangle and noise voices. NES cartridges retain their mixed APU output. Audible quality needs listening checks.
- [Bitty Buddy](BITTY_BUDDY.md) adds connected course sections, coin routes, animated magnets, flight, score boosts, pocket prizes, run goals, trick chains and buffered actions. The character faces down the course during play. [The mechanics audit](BUDDY_MECHANICS_AUDIT.md) records the private reference evidence and remaining gaps.
- [Mario’s clock](MARIO_CLOCK.md) runs the supplied NTSC cartridge’s actual engine through four authored courses. Native physics handle enemies, orange lifts, swimming, pipes, flags and death. A portrait crop retains native pixels. Manual play reloads the original cartridge without the home’s cached course patches.
- Every game has a portrait clock home, stacked hours and minutes without colons, shared 12/24-hour formatting, and optional weekday/date placement. Homes leave scores and rewards untouched.

## Current StickS3 measurements

These October 29 measurements apply to the current app-only gameplay candidate. No Bluetooth controller was connected. Diagnostics injected the Geometry Dash held-action input. The clock tests used their automatic home directors; the Hockey test used its solo CPU mode.

| Scenario | 500 completed LCD transfer intervals | Completed transfer rate |
| --- | ---: | ---: |
| Native-engine Mario clock | 8.335464 s | 59.98 FPS |
| Bitty Buddy clock | 8.333589 s | 60.00 FPS |
| Geometry Dash with injected held action | 8.333143 s | 60.00 FPS |
| Hockey Queen solo | 8.333452 s | 60.00 FPS |

Each interval window also recorded 500 simulation frames. The source and completed-transfer counters are independent. StickS3 uses a separate transfer snapshot so rendering can overlap the previous LCD transfer. These measurements establish throughput in the named scenarios. Panel tearing, physical controls and sound remain separate checks.

Geometry Dash reached its native gameplay state without a controller. The timed USB diagnostic launch took 12.9 seconds, including repeated status queries. A private host probe exercised the same launcher in 292 presentations and confirmed that action input changes the native player trajectory. The host audio trace produced 850,400 mixed 24 kHz samples with no clipped samples in that trace.

The splash diagnostic measured 2,401 ms and 144 animation frames. Device framebuffer captures cover the main menu, colored game picker, Mario clock and Buddy clock. A restart returned to the last selected game with the recorded profile and arcade scores unchanged. The speaker driver and IMU report ready. Fresh tilt calibration completed during the solo Hockey test. Those status reports do not establish audible output or correct physical movement direction.

## Source and reference checks

The source suite covers Python imports and publication checks, portable C++ state machines, interruption-safe cartridge storage, renderer bounds, input chords, launch calibration, CPU play and asynchronous display ownership. Portable runtime checks use AddressSanitizer and UndefinedBehaviorSanitizer. NES checks cover CPU, PPU, APU, NTSC scheduling, input, program caching and video paths. The browser transfer tests use mocked Web Serial sessions.

Private supplied-ROM traces separately verify the native Mario course director. A 16,000-frame trace visited all four area types, completed ten courses, entered six pipes and reached four goals. It observed native orange-platform actors. Mocked director tests exercise death and recovery paths. The earlier 6,000-frame manual Mario crop comparison retained identical CPU RAM and displayed pixels through 5,152 moving-platform actor-frames. Physical lift play remains an acceptance item.

Public Plus and StickS3 USB-library candidates compile from authored inputs with commercial ROM/APK discovery disabled. The public application files are approximately 1.54 MB and 1.55 MB, respectively, within their 2 MiB application slots. Private embedded-cartridge builds remain separate and are never placed in a public installer bundle. Explicit source-package and Git-history audits enforce the publication boundary.

## Earlier hardware evidence

The original Plus has a verified full 4 MiB recovery backup. Earlier app-only updates retained its partition table, settings, controller bonds and records. Its earlier longer-splash build measured 2,400 ms and approximately 60.009 completed SPI boundaries per second in separate Mario clock and manual-play probes. That clock used the earlier scene implementation.

The earlier private Jetpack placement build passed on-foot, dense magnet-coin, Dragon and Rocket Time probes. Across 2,000 completed LCD intervals it averaged 59.9944 FPS with Full volume and BLE discovery active, with no controller connected. Records and profile remained unchanged. Remaining reference-comparison work is recorded in [the Jetpack audit](JETPACK_AUDIT.md).

## Required before public firmware installation

1. Retain verified recovery backups and prove restoration before partition migration.
2. Test the USB-library layout while preserving needed settings and controller bonds.
3. Test fresh browser installation, runtime upload, interruption, removal, replacement, restart, last-game restoration and controller reconnection on each supported board.
4. Measure runtime-loaded games separately from private embedded-cartridge builds.
5. Check physical buttons, tilt direction, audible sound, battery sleep/wake and gameplay presentation. Test both sticks together for radio discovery, input, authoritative snapshots, rematches and reconnection.
6. Publish linked-library notices, corresponding sources and relinking materials with downloadable firmware.

The public site at [bitty.games](https://bitty.games) is maintained separately. This repository supplies the installer and engineering docs. The site maintains its community, Sponsors and hardware links, with compatibility labels and paid-link disclosures.
