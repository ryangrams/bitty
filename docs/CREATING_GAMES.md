# Create a Bitty game or app

Start with a tiny playable idea, a clear control scheme and an animated clock home. Original games and useful apps are welcome. Open a [game or app request](https://github.com/ryangrams/bitty/issues/new?template=port-request.yml) before a large contribution so the device, controls and distribution plan can be reviewed. For agent-assisted work, use the [agent quickstart](AGENT_QUICKSTART.md).

## Choose how it runs

| Contribution | How it reaches the device |
| --- | --- |
| An original native C++ game or app | Integrate its source, compile the firmware and install a board-specific firmware update. |
| A compatible `.nes` cartridge | Supply a local file through the USB library. Generic NES support and specialized adapters have different limits. |
| A custom Flappy skin or other supported asset import | Use the existing local import tools and rebuild. These are build-time inputs. |
| A community-hosted project | Propose an external source/download link with its license and target details for review. |

Resident native engines share one application image. A cartridge upload stores data for an existing engine. Hiding a native game cannot recover its compiled code space. There is currently no arbitrary native executable plug-in format, executable marketplace or automatic external-game installer.

The [USB library guide](USB_LIBRARY.md) records supported cartridge mappers, prepared-image requirements, file sizes and recovery behavior. [Release status](RELEASE_STATUS.md) identifies which browser and device paths have physical evidence.

## Current inputs and artwork

| Game | Public input and artwork status |
| --- | --- |
| Flappy Bird | Public builds use authored starter art. Original-game art and audio require a lawful private import and stay outside the public build. |
| Mario and other NES games | Users supply a compatible local ROM. Mario clock art is decoded from the supplied recognized NTSC image. |
| Tetris | Users supply the supported ROM and patches through local preparation. Clock graphics are authored geometric blocks. |
| Geometry Dash | Users supply the supported prepared Famidash image. Its adapter and authored native renderer do not include the cartridge or its music. |
| Galaga and Arkanoid | Resident source uses authored art and generated courses, with no user file required. Their reference names and closely inspired presentation still need a distribution review. |
| Bitty Jet | Resident source uses authored art, effects and formations in public builds. Optional APK-derived formations or sprites are private imports. Earlier code and import paths retain Jetpack identifiers. |

Authored assets and a new title describe what the build contains. They do not establish clearance for every aspect of a finished game. Check the [rights boundary](RIGHTS.md), upstream licenses and the actual exported files before publishing source or firmware.

## Design for the actual board

The baseline is the original ESP32 M5StickC Plus: a 135 by 240 display, 4 MiB flash and no PSRAM. Its left-side power key supports a short-tap action while retaining its hardware power function on a long hold. Build the interaction around the front M5 action button, short side-button taps and optional tilt. Keep controller input optional where the game allows it.

The separate [StickS3 target](STICKS3_AUDIO.md) has different hardware and audio. Its available memory, reset button and controller behavior require board-specific checks. A matching screen size alone does not establish compatibility. Use the [development skill](../skills/m5stick-development/SKILL.md) and [hardware reference](../skills/m5stick-development/references/hardware.md) before assigning pins or power behavior.

Keep pixels sharp with native bitmap text and integer sprite scales. Inspect a 135 by 240 image at its actual size. Gameplay can use landscape when appropriate; the shared clock home uses portrait orientation.

## Follow the shared home policy

Every resident game or app needs a clock home. [GameHome](../firmware/flappy/game_home.h) and [ClockRuntime](../firmware/flappy/clock_runtime.h) own the common flow:

1. Booting or selecting a game opens its portrait clock home. Selecting the current game requests home again.
2. On home, a short left tap advances to the next available game or cartridge. The front M5 action starts manual play.
3. Hold right to open the system menu. Within it, right taps advance the selection and M5 selects. Hold right to exit.
4. **Game home** returns to the current game's clock. **Set clock** belongs in the shared menu; a separate Clock mode selector is unnecessary.

Home animation should keep moving safely, display local time through the game's objects and leave scores, currency, missions and other saved progress untouched. The shared code releases gameplay resources before allocating the selected clock scene. Preserve that separation.

Implement a small scene class with this shape:

```cpp
class MyClock {
 public:
  void reset(uint32_t seed);
  void step(const flappy::ClockTime& time);
  void draw(uint16_t* pixels, const flappy::ClockTime& time) const;
};
```

[clock_scene.h](../firmware/flappy/clock_scene.h) supplies the 135 by 240 dimensions, time digits, format handling and clipped rectangle helpers. The caller supplies the RGB565 canvas and advances the scene at 60 fixed steps per second. Keep `draw` independent of simulation state. Borrow the existing canvas, keep scene state small and avoid allocations during `step` and `draw`. Reserve the bottom 15 rows for the common home controls footer.

Use [Jetpack Clock](../firmware/flappy/jetpack_clock.h) for scrolling objects and collection animation, [Geometry Clock](../firmware/flappy/geometry_clock.h) for smoothly eased motion, or [Tetris Clock](../firmware/flappy/tetris_clock.h) for a scene with several phases. These classes implement the home presentation separately from their playable engines.

## Add a native engine

The native engines are portable C++ state machines. [GalagaGame](../firmware/flappy/galaga_game.h), [ArkanoidGame](../firmware/flappy/arkanoid_game.h) and [ArcadeInput](../firmware/flappy/arcade_support.h) show the existing approach: reset from a seed, tick from bounded input, draw into the supplied canvas and expose event cues. Use that pattern when integrating another engine into a firmware build.

Keep game rules separate from M5 APIs. Use fixed simulation steps, bounded catch-up and deterministic seeds. Rendering less often must not speed up the game. Slow-motion should scale motion integration while retaining smooth rendering. Drain display transfers before reusing borrowed buffers or changing orientation.

Integrate the new engine through the current source paths:

| File | Responsibility |
| --- | --- |
| [app_model.h](../firmware/flappy/app_model.h) | Game identity, availability, picker entries, per-game settings and requests. Preserve existing persisted enum values. |
| [clock_runtime.h](../firmware/flappy/clock_runtime.h) | Create, step, draw and release the new clock scene. |
| [flappy.ino](../firmware/flappy/flappy.ino) | Allocate and release the playable engine, map input, schedule ticks, display frames, route audio and handle pause/sleep/game changes. |
| [tools/test.py](../tools/test.py) | Register portable tests and source-only generated fixtures where needed. |
| [PUBLIC_FILES.txt](../PUBLIC_FILES.txt) | Include reviewed source, tests and documentation in the public export. |

The build stages firmware headers automatically, but menu, lifecycle and runtime dispatch still require explicit integration. Put game-specific options under **Game Settings**. Reuse shared brightness, volume, controller, clock and sleep settings. Verify action press/release edges, held controller chords and input clearing after a menu or game switch.

## Budget storage and test behavior

The original Plus USB-library layout reserves 2 MiB for the application. Its remaining mapped library holds cartridge data, with up to twelve directory records and 4 KiB allocation pages. Replacing a cartridge temporarily needs enough additional contiguous free space for the new payload. Read [USB storage and interruption behavior](USB_LIBRARY.md#storage-and-interruption-behavior) before changing this layout.

Use the [quickstart checks](AGENT_QUICKSTART.md#check-the-result), then compile the exact board target. Add tests for simulation rules, buffer guards, deterministic reset, time/format rollover, long autoplay, game switching and score/profile isolation. Source-only tests must use authored or synthetic inputs. Keep proprietary reference comparisons private and label their evidence separately.

Measure simulation time and completed display transfers independently on the device. Test real controls, sound, battery sleep/wake and controller reconnection after the relevant changes. Keep an existing flash backup before partition changes. Record hardware, input source, scenario, build identity and limitations with results.

## Share a project people can review

Bitty source uses the [MIT license](../LICENSE), with separate [upstream notices](../THIRD_PARTY_NOTICES.md). Supply original or suitably licensed code, artwork and audio. Users must have the rights or permission needed for the games and assets they load. A rights checkbox records their statement; it supplies no license. See the [rights boundary](RIGHTS.md).

Keep commercial ROMs, APKs, extracted media and builds containing them out of public commits, history and attachments. If an integration needs user-owned files, document the accepted input and a local preparation path. Use [the source exporter](DISTRIBUTION.md) for public source packages.

For a creator-hosted cartridge or independent project, submit an external link in a GitHub request or contribution. Include:

- Project name, author, source repository and release tag or commit.
- Code and asset licenses, attribution and provenance.
- Exact target hardware, controls and build instructions.
- Download format, size and checksum, plus any required resident adapter.
- Tested behavior and the distinction between host checks and physical-device checks.

Review can establish whether a link belongs in the project. It does not make its payload compatible with Bitty. The current USB manager installs accepted local files; an external catalog or automatic download/install flow would need its own implementation and review.
