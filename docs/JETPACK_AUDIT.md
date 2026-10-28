# Jetpack vehicle and bonus audit

Magnet coins now travel toward the runner, Boost keeps coin formations in play, and Cash Dash ends with falling coins you still have to catch. The current source also adds the recovered vehicle controls, a complete Rocket Time sequence and animated Final Spin reels.

This audit describes the current vehicle and bonus changes. The current firmware passed new device timing and sequence checks. Twelve active-gameplay probes averaged 59.9667 completed LCD transfers per second across 6,000 intervals. See [verification details](JETPACK.md#verification) for test conditions and limits.

## Evidence and limits

The behavioral reference is the user's Android 1.104.2 package. Its packaged settings and static behavior analysis supplied the control equations, timing and reward values. No live Android trajectory comparison has been performed. The M5 uses independently written code and authored pixel art.

The public source contains behavioral findings, tests and import tools. Package files, extracted artwork, missile formations, decoded settings and native analysis remain private. A [local import](JETPACK_IMPORT.md) can supply original formations. Source builds retain authored fallback patterns.

## Coins and pickups

- **Magnet:** nearby coins enter a visible attraction pool. Credit occurs on contact with the runner. An imported formation keeps its point available until collection or successful transfer into that pool. A full pool therefore leaves the point in the formation. The 65-pixel attraction radius is an M5 fit; the original default radius remains unresolved.
- **Boost:** ordinary coin formations continue spawning while hazards remain cleared. Collection checks swept horizontal contact during fast scrolling. Existing coins also survive Rocket Time activation.
- **Cash Dash:** the path supplies one coin per traveled meter with the configured gem chance. You must catch the endpoint to start its 300-to-700-coin shower. Falling entries have spray velocity, gravity, drag, bounces and a 0.3-second collection lock. After that lock, Magnet can attract them. Entries that leave the playfield are lost.
- **S.A.M.:** coin contact follows the fixed body. The moving fist blocks missiles.

Magnet and Cash Dash share at most **48 active value particles**. A shower particle can represent several source coins, with each coin's gem roll included in its stored value. This bounds memory and preserves the value of a collected bundle. It changes the number of independently catchable coins.

## Vehicles

| Ride | Current correction | Remaining boundary |
| --- | --- | --- |
| Gravity Suit | A fresh tap reverses gravity while retaining momentum. | Collision and screen proportions use M5 fits. |
| Hog | First-step jump impulse, held extension, release cancellation and buffered landing taps. | Final fitted speed is capped for this display. |
| Stomper | First-step impulse, longer held boost, descent assistance and buffered landing taps. | Entry and ejection remain abbreviated. |
| Profit Bird | Flap impulse adds to momentum and uses a ten-released-tick cooldown. | Its configured horizontal profile has not been verified through the original wrapper. |
| Teleporter | Departure and arrival phases, a paused departure marker, and an arcing target with longer edge dwell. | Nominal twelve-tick phases can differ from the original floating-point animation boundary. |
| Dragon | Hold to dive, release to rise, with its configured vertical speed limit. | Roof return, articulated body and tail behavior remain incomplete. Its horizontal wrapper is also unverified. |
| S.A.M. | Fixed body, jetpack-driven fist, reduced fist ceiling and its own speed profile. | Body and fist collision proportions are authored for this screen. |

Vehicle speed profiles now include their initial deceleration or start clamp and sine ramp. The final vehicle speed, after difficulty scaling, is capped at **1000 native units per second**. This particularly limits the Hog's source maximum. Boost has a separate additive speed model. Exact pickup construction, transition particles, camera choreography and ejection impulses remain open.

Practice keeps incompatible starting states separate. Selecting a ride clears headstart and the Boost, Cash Dash or Rocket Time starting bonus. Selecting one of those bonuses clears the starting ride. Headstart also clears the starting ride and those three bonuses. Magnet, Double Coins and Shield can still accompany a practice ride. Practice earnings remain isolated from saved progress.

## Rocket Time

Rocket Time eases the world, flight forces, drag and displacement into slow motion, then eases them back out. Its sequence clock uses unscaled ticks. The slow-motion target is captured from running speed at activation, so crossing a speed threshold later leaves that target unchanged.

The sequence schedules missile waves, their chase and warning phases, launch timing and angled or ricocheting flight. The private import can supply the package's 15 patterns and 88 missile entries. The public build uses authored fallback waves. Ordinary coin groups pause during Rocket Time, while existing coins remain collectible. Missiles selected for a trail emit gems at scaled intervals at every upgrade tier.

Upgrades offer one through six distinct classic vehicle choices near the end. Readable M5 lanes replace the source's overlapping placement. Collecting a choice ends Rocket Time and restores ordinary motion before the ride starts. Ending or replacing the power also clears its remaining missiles and choices. Premium ownership and premium choices remain outside the current catalog. Decorative animation and scientist walking retain their existing animation cadence during slow motion.

## Final Spin and crash travel

Final Spin now shows three clipped, scrolling reels with staggered stops over 60 simulation ticks. The result follows the reel animation. Wins hold for 150 ticks and losses for eight, with a fresh action available to continue. Cash-in remains available before a spin, at 100 coins per remaining token. The reward ledger, reduced repeat weights and forced-win countdown remain separate from the visual reels.

The reels use authored presentation timing. The original finite-drum construction and touch-stop phases remain incomplete, so exact overall reel odds are unverified. Blast rewards return to the moving crash sequence. The run waits for vertical settling and horizontal speed to reach zero before returning to Spin or ending.

## Checks and open work

Portable tests cover vehicle equations and speed caps, scaled flight, Rocket events and cancellation, missile chase and reflection, distinct choices, shower lockout and Magnet interaction, swept coin contact, reel clipping and result timing, and saved-profile isolation. The importer uses synthetic packages for public checks. Private checks can use the supplied formations without placing them in the source package.

The remaining work includes Dragon behavior, full vehicle transitions, a live reference comparison, exact reel odds, per-vehicle magnet equipment, the full gadget and premium catalog, and physical playthrough, sound and controller-feel verification. See [progression](JETPACK_PROGRESSION.md) for reward and upgrade values.
