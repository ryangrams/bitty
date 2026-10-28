# Mario camera and moving lifts

The orange platforms could disappear below the display while Mario was still standing on them. The old camera stopped at NES row 223. A vertically moving platform reaches lower rows, so its sprite and Mario's feet could be cut off even while native collisions kept working.

The view still copies each NES pixel directly. Its 240 by 135 image reserves 16 rows for the HUD and 119 for the world. The usual world window starts at row 105. It can now reach row 239 when a low platform or Mario's feet need that space.

On recognized cartridges, the camera reads platform coordinates from the game's RAM. It selects a nearby surface beneath Mario and includes the whole eight-row platform when the player and surface fit together. Small lifts have two surfaces, 128 NES rows apart. Player size, horizontal reach, enemy identity and vertical page are checked. Animation pixels and invincibility blinking do not select the camera target.

This changes the displayed window. NES platform motion, collisions, input, NTSC timing and frame delivery keep their existing paths. When a platform and Mario span more than the world's 119 rows, some source rows still fall outside the crop.

## Verification

Source-only tests move synthetic platforms through the lower edge, ride them, jump, change player size and alternate sprite visibility. They compare every output pixel with the source through camera changes and eight-row callback boundaries. Tests also cover both small-lift surfaces, invalid targets and the normal ground height. The tests pass with AddressSanitizer and UndefinedBehaviorSanitizer on macOS Clang and Linux GCC.

A private probe using the supplied NTSC cartridge covers a vertically oscillating platform, a rising lift and a falling lift. Across 3,885 emulated frames, full and optimized paths have identical CPU RAM, and cropped pixels and simulated LCD updates match the full image. In 617 sampled frames where the landing surface and player fit together, the old crop clipped the platform in 122 frames. The updated crop clips none of those samples.

An independent JSNES run matches the platform identities, positions and speeds across 2,544 platform actor-frames. Player state sampled at input transitions differs between the cores, so this is a platform trajectory comparison. It does not establish complete emulator equivalence.

The private probe uses scene setup to reach each lift, followed by NES button input for riding and jumping. Its ROM, setup and captures stay outside the public source package. No test scene writes are included in firmware. This candidate has not been flashed or physically playtested.
