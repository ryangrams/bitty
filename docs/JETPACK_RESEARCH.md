# Jetpack reference and M5 design

Research checked on October 27, 2026. This records observed behavior and proposed design choices. The M5 implementation uses original code and graphics.

## What the references establish

The [Flipper remake README](https://github.com/timstrasser/flipper-jetpack-game) credits Halfbrick and describes a Jetpack Joyride remake. Its [gameplay screenshot](https://github.com/timstrasser/flipper-jetpack-game/blob/master/docs/screenshots/gameplay.png) shows a small outlined runner, lab panels, curved coin trails, floor scientists, and a large missile. The monochrome composition is easy to read. The [main loop](https://github.com/timstrasser/flipper-jetpack-game/blob/master/jetpack.c) supports coins, missiles, scientists, exhaust particles, saved best distance, and lifetime coins. It does not dispatch zapper, laser, or vehicle behavior. Treat those as additions for our adaptation.

Halfbrick's [original launch announcement](https://www.halfbrick.com/blog/jetpack-joyride-sep-1st) identifies coins, vehicles, missions, and jetpack variety as central features. The [iPhone listing](https://apps.apple.com/us/app/jetpack-joyride/id457446957) confirms one-touch controls, zappers, lasers, guided missiles, vehicles, and power-ups. This supports a clear priority: responsive flight, readable danger, satisfying collections, and changing movement through vehicles.

Halfbrick's [vehicle help page](https://halfbrick.helpshift.com/hc/en/4-jetpack-joyride/faq/689-how-do-i-upgrade-vehicles/) lists six default vehicles: Bad As Hog, Crazy Freaking Teleporter, Gravity Suit, Lil' Stomper, Mr. Cuddles, and Profit Bird. It describes vehicle pickups as rainbow boxes with gears and says the vehicles have distinct controls. The [power-up help page](https://halfbrick.helpshift.com/hc/en/4-jetpack-joyride/faq/706-how-do-i-upgrade-power-ups-1692683521/) distinguishes temporary shields, boosts, coin trails, and slow-motion missile sequences from those vehicles.

These pages do not publish authoritative gravity, acceleration, hitbox, or spawn constants. Our tuning values must be identified as M5 design choices and checked in simulation. Avoid treating third-party recreation specifications as original game data.

## M5 direction: an SNES-style game

Use the full 240 by 135 landscape screen. Draw at native pixels with a fixed camera. Give the runner roughly the left fifth of the screen so incoming hazards have most of the width for warning. A compact upper HUD can show distance, coins, and a vehicle icon. Keep the floor shallow. This preserves vertical flight room and removes the need for camera movement.

Use an original compact runner with distinct standing, running, thrusting, and crashing poses. Keep the sprite silhouette visible through the exhaust. Draw lab walls from small repeating panels, with a slower background layer and a faster floor layer. All movement can use subpixel simulation while rendering on whole pixels.

Choose a stable palette: dark blue lab walls, steel and teal machinery, warm skin, amber coins, orange exhaust, bright cyan electricity, and red missile warnings. Give hair, fabric and machinery deliberate shadow, base and highlight clusters. Use a one-pixel dark outline where the background could hide an object. These choices define the SNES-inspired art direction.

Prioritize readable animation: coin glint, a few running frames, alternating electric arcs, missile exhaust, a brief pickup burst, and a visible crash. Simple repeating pixel shapes will read more clearly than small gradients or shrinking phone artwork. Reuse the existing native menu font without stretching it.

## Controls, fairness, and scope

Hold front M5 or controller A/B to thrust. Release to descend. Use the existing long right-button menu behavior and controller pause convention. A restart must require a fresh press so the input that caused a crash cannot immediately restart the run.

Run fixed 60 Hz physics independently from display timing. Bound upward and downward velocity, make the floor and ceiling safe surfaces, and measure obstacle clearance against the runner's visible body. Keep particles decorative. Model each electric beam with a narrow visible collision segment and each missile with a visible body hitbox. A missile warning should establish the lane before firing and remain distinguishable by shape as well as color.

Build obstacle groups with reachable safe corridors. Test both maximum thrust and full release trajectories before combining hazards. Keep extra warning distance as horizontal speed increases. Keep a fixed camera and avoid screen shake during ordinary play.

The expanded port includes the six standard vehicle concepts and S.A.M., the four current power-up concepts, coin bonuses, scientists, spin rewards and moving crash sequences. Halfbrick's [S.A.M. announcement](https://www.halfbrick.com/blog/jetpack-joyride-1-6-frequently-asked-questions) establishes the S-A-M letter-token activation. The M5 allows repeat offline activation and direct practice selection. It has no daily lock or advertising requirement.

The M5 includes a coin bank, powerup upgrades and a compatible mission subset. Ads, online leaderboards, licensed crossover vehicles, event economies, cosmetic catalogs and the complete gadget catalog remain outside this port. New vehicle controls and collisions use M5 adaptations informed by package configuration. Their complete native update functions have not been independently recovered or verified against a running phone. Normal jetpack and Gravity Suit behavior have the stronger native evidence described below.

## Provenance

A privately supplied Android 1.104.2 bundle was also inspected locally. Its configuration and native Mortar engine establish normal gravity 0.15, held thrust 0.36, and per-reference-frame drag 0.98. At 60 Hz, the normal flight update adds the force, applies drag, and integrates velocity. The force grows with the running speed. Native normal-flight center limits are directly clamped to -120 and +120. These observations come from configuration decoding and native analysis; they have not been checked against a live Android run.

The classic running-speed curve is linear in integer meters: it begins at 175 native units per second and reaches 480 after 2,250 meters. The distance counter divides native traveled distance by 18. The M5 adaptation fits the 240-unit vertical center range into its 92-pixel flight range. That chosen fit maps normal running speed to about 67.08 through 184 screen pixels per second. The original phone viewport dimensions and adaptive aspect behavior were not established, so this fit is an M5 design choice.

The mobile configuration and scheduling code establish meter-based pickup buckets, vehicle anti-repeat weights, locked content profiles, spin-token guarantees, SAM compensation and bonus weights. These now drive the M5 progression system. See [the progression audit](JETPACK_PROGRESSION.md) for values and adaptation boundaries. The Gravity Suit update confirms momentum-preserving tap reversals, gravity 0.26 and drag 0.98. Those observations guide its M5 movement. The M5 keeps its own generated obstacle layouts, warning system and recovery windows. Its small collision capsule is a forgiving approximation of the recovered normal 32-by-32 collision rectangle. Default source builds include only project-created coin formations. A [private import](JETPACK_IMPORT.md) can rebuild original coin coordinates from a locally supplied package. Original pattern data remains outside the public package.

The original sprite sheets were viewed privately to check proportions, separate head/body animation, hair and clothing. The normal M5 renderer uses project-created pixel art. The private importer also offers an optional original runner reconstruction with explicitly approximate animation mapping. APK files, imported pixels and patterns, decoded configuration files, decompiled Java, native libraries and disassembly stay in ignored local storage. The public package includes the original implementation, behavioral tests, import tools and these research findings.

At this check, the [Flipper repository metadata](https://api.github.com/repos/timstrasser/flipper-jetpack-game) reports no license, and its [recursive file tree](https://api.github.com/repos/timstrasser/flipper-jetpack-game/git/trees/master?recursive=1) contains no license or copying notice. GitHub's [licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) explains that public visibility alone does not grant general redistribution or derivative-work rights. Keep Flipper code and sprite files out of the public package unless a suitable license or permission is obtained. Credit it as a visual and gameplay reference. Keep Halfbrick artwork, audio, logos, and character sprites out of the package as well.
