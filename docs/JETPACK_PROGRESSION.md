# Jetpack progression

The reference for these values is the user's Jetpack Joyride Android 1.104.2 package. The port keeps its own code and pixel art. Private package files, extracted assets and disassembly stay outside the distributed source.

## Course pacing and pickup order

`jetpack_progression.h` owns a deterministic shared content scheduler. A completed content slot first checks a due powerup, then a due vehicle, then the current course profile. Coins, zappers, missiles and lasers share those slots. Spin tokens and SAM letters use separate checks at content boundaries.

The native scheduler compares pickup targets against the player's distance plus a 100m prefetch window. That target is an eligibility threshold. The actor uses the ordinary offscreen spawn anchor. The M5 records the actual expected encounter distance separately from the manager's due meter. Pickup periods use displayed meters. Course interval values use native world units, with 18 units per displayed meter. Screen fitting uses 23/60 of a pixel per native unit.

- The first powerup uses a 300m bucket. Later buckets draw a fresh length from 500 through 999m.
- Vehicles use 650m buckets, with a 470m exclusion after a vehicle is destroyed.
- A selected vehicle's repeat multiplier becomes zero. The other five recover by 0.25, capped at one.
- Default course slots can start missile streaks at 0.35% or coin streaks at 0.15%. These streaks span four through seven and two through three intervals respectively.
- Spin tokens use a 2.8% boundary chance, at most three per kilometer, with a 1050m guarantee and a randomized first guarantee from 200 through 449m.
- SAM uses its own 2.8% check, first-letter guarantee, spacing and saved distance compensation. Collected letters survive eligible run boundaries. The M5 permits offline activation without the source's daily eligibility service.

The course changes its weighted mix and spacing as distance increases. Each vehicle also changes the content profile. The screen adaptation adds a reaction gap for hazards and a recovery gap after powerups or vehicle loss. These gaps are explicit M5 adjustments. A bounded event trace records the chosen type, source distance and intended encounter distance for tests and diagnostics.

## Final Spin

`jetpack_rewards.h` defines ten prize outcomes. The package has two identical 500m headstart rows with weight 100 each. The helper combines them into one row with weight 200.

| Prize | Award | Initial weight | Weight after winning |
|---|---:|---:|---:|
| Small coins | 200 coins | 120 | 120 |
| Medium coins | 1000 coins | 90 | 90 |
| Large coins | 2000 coins | 50 | 50 |
| Small blast | Tier 1 | 120 | 60 |
| Medium blast | Tier 2 | 75 | 18 |
| Large blast | Tier 3 | 40 | 4 |
| Second chance | Revive current run | 100 | 2 |
| More tokens | 3 tokens | 70 | 70 |
| Double coins | Next run | 120 | 0 |
| Headstart | 500m next run | 200 | 0 |

The initial weight sum is 985. After a win, the reference calculates `floor(initialWeight * afterWinProbPercent)`. Every later win applies that same fixed result. The multiplier has a default of 1. A second chance can appear again at its reduced weight. Double coins and headstart each disappear from that run's prize selection after being won.

The configuration labels `COINS_100`, `COINS_500` and `COINS_1000` resolve to 200, 1000 and 2000 coins in this package. The labels' numeric suffixes are historical names.

Each remaining token can be cashed in for 100 coins. `cashInValue()` uses a 32-bit result. A token prize adds three tokens after the spent token has been deducted.

`FinalSpin::spin()` includes a no-prize outcome. Each of the two later reels has a 50% target-match chance. An ordinary spin needs both matches to win. A forced-win countdown starts at three or four spent tokens. It decreases once per spin, guarantees a matching result when it expires, then starts another three- or four-spin cycle. An ordinary win keeps the current countdown.

The original builds finite drums of eight weighted symbols, arranges them to avoid adjacent duplicates where possible and accepts timed touch stops. Its ordinary stop policy has a 50% center target, a 25% next symbol and a 25% previous symbol. The M5 converts that policy into two match checks and a compact result screen. Its finite-drum arrangement and touch-stop phase are adapted, so this implementation does not claim the original's exact overall return. `draw()` remains available for selecting a weighted winning prize after a match has been established.

`FinalSpin::Eligibility` lets the caller exclude a revive outside a recoverable run, or a next-run consumable that cannot be accepted. Excluding an outcome preserves its stored weight. Call `resetWeights()` for a new run. A revive continues the same run and keeps the reduced weights. Full `reset(randomWord)` initializes a loaded game session. The forced-win countdown survives run retries and revives within that session, matching the reference object's lifetime.

Native blast velocity is `(tier + 1)` times the base values X=500 and Y=6. The three results are X=1000/1500/2000 and Y=12/18/24. The M5 maps the 2:3:4 strength ratio onto its small playfield.

Evidence in the supplied arm64 library:

- `Slot::ParseProbability`, `0x1037cec`: reads the initial weight and post-win multiplier.
- `Slot::SlotTypeWin`, `0x1039434`: multiplies the initial weight and truncates to an integer.
- `Slot::ResetProbabilities`, `0x103971c`: restores initial weights.
- `SlotMachine::GiveReward`, `0x103e8ac`: resolves coin payouts, adds three tokens and grants consumables.
- `SlotMachine::CashInTokens`, `0x103c938`: multiplies remaining tokens by the configured cash-in value.
- `SlotMachine::SlotMachine`, `0x103a1ac`, and `UpdateLeverState`, `0x103e108`: initialize and decrease the forced-win countdown.
- `SlotMachine::Update`, `0x103cc18`: checks all three symbols and resets an expired countdown.
- `Slot::Randomise`, `0x1036efc`, and `GetDICTSlotType`, `0x10382dc`: build and arrange the finite drums.
- `Player::Explode`, `0xebefdc`: applies blast tier strength.

## Coins and saved rewards

A run's coin balance includes gameplay coins, coin prizes and cashed-in tokens. Checkpoints credit only the increase since the last checkpoint. Settlement credits any remaining increase and records the completed distance once. A second chance keeps the same run balance and checkpoint. Save earned headstarts and double-coin runs as inventory, then consume one when the next eligible run starts.

Practice and diagnostic runs cannot change the saved wallet, mission progress, SAM letters or reward inventory. A restored profile retains completed missions, total mission progress and earned rank rewards. Partial single-run objectives restart after a reboot.

`JetpackMissions::Stats` uses cumulative values within a run. Repeated observations add no progress. A revive retains those values. Gameplay coin and field-token counters exclude Final Spin prizes and token cash-in so each objective has a clear input.

## Powerup upgrades

The table uses the package's `#default` configuration. The port starts each powerup at its baseline level. Baseline ownership is an M5 adaptation. Buying an upgrade charges the next level's cost.

| Powerup | Level costs in the package | Effect at each level |
|---|---|---|
| Shield | 7500, 45000, 185000, 625000, 1200000 | 1, 2, 3, 4, 5 hits |
| Boost | 1500, 3000, 17500, 85000, 425000 | 100, 200, 300, 400, 500m |
| Cash Dash | 0, 12500, 65000, 225000, 650000 | 60, 80, 100, 120, 150m |
| Rocket Time | 1000, 3500, 12000, 30000, 75000, 150000, 250000, 350000, 450000, 550000 | 7, 9.5, 12, 14.5, 17, 19.5, 22, 24.5, 27, 29.5 seconds |

Cash Dash's source coin payload rises from 300 to 700 in steps of 100. Cash Dash credits its caught payload through a bounded burst effect; it does not allocate hundreds of separate coin bodies. Its gem percentage is 5/10/15/20/25. Boost native additive speed is 2000/2000/3000/3500/3500. It approaches that modifier by 35 native units per reference tick. The boosted state clears at the endpoint minus 30/30/40/50/50m, then the modifier decays by 0.9 per tick. Rocket Time uses a 0.5 time multiplier. Its campaign pickup and purchase unlock at mission rank 5. Practice can exercise it earlier. These values are distinct from any conversion needed for the M5's rendering scale.

## Three active missions

The tracker keeps three objectives active. Completed slots stay visible until the next run, then advance through this compatible subset. Stars match the source difficulty field. Labels are newly written and fit 21 characters.

| Order | Objective | Scope | Stars | Source mission ID |
|---:|---|---|---:|---:|
| 1 | Gather 150 coins | Total while active | 1 | 122 |
| 2 | Reach 1000m | One run | 2 | 94 |
| 3 | Find 2 vehicles | Total while active | 1 | 115 |
| 4 | High-five 10 scientists | One run | 1 | 11 |
| 5 | Find 2 spin tokens | One run | 1 | 3 |
| 6 | Run 300m on foot | One run | 2 | 19 |
| 7 | Find 3 Mechs | Total while active | 1 | 10 |
| 8 | Cash in 3 tokens | Total while active | 1 | 66 |
| 9 | Start 5 runs | Total while active | 1 | 7 |
| 10 | Buy an upgrade | One purchase | 2 | 9 |
| 11 | Gather 200 coins | One run | 1 | 13 |
| 12 | Travel 500m before a vehicle | One run | 2 | 5 |
| 13 | Travel 3000m | Total while active | 1 | 72 |
| 14 | Ride the Hog for 750m | Total while active | 2 | 17 |
| 15 | Gather 5 spin tokens | Total while active | 1 | 16 |
| 16 | Flip gravity 30 times | One run | 1 | 18 |
| 17 | Win 5 Final Spin prizes | Total while active | 3 | 20 |

Foot distance counts ground travel with no vehicle. Hog distance counts travel in the Hog. Vehicle-free progress stops if the player collects a vehicle before reaching 500m. The gravity objective counts flips across the current run. The reference resets that objective when the Gravity Suit is destroyed. Counting across the run is an M5 adaptation.

The source includes objectives for zapper near misses, flying over zappers and touching red alarm lamps. Those event types are omitted from this subset. After the final objective, the sequence repeats. The tracker caps rank payouts at the source's 15 entries.

| Rank completed | Stars required | Coin reward |
|---:|---:|---:|
| 1 | 3 | 400 |
| 2 | 3 | 800 |
| 3 | 3 | 1200 |
| 4 | 5 | 1600 |
| 5 | 5 | 2000 |
| 6 | 7 | 2400 |
| 7 | 7 | 2800 |
| 8 | 7 | 3200 |
| 9 | 9 | 3600 |
| 10 | 9 | 4000 |
| 11 | 11 | 4400 |
| 12 | 11 | 4800 |
| 13 | 11 | 5200 |
| 14 | 13 | 5600 |
| 15 | 13 | 6000 |

Each completed objective grants stars once. Surplus stars carry to the next rank. The rank table totals 117 stars and 48000 coins. `takeCoinReward()` drains queued rank coins once so the profile can credit and persist them together.

## Portable checks

`jetpack_rewards_test.cpp` exhausts every initial weighted ticket, every post-win ticket and 10000 ordinary stop combinations. It verifies fixed repeat suppression, no-prize outcomes, forced-win cycles, eligibility and cash-in arithmetic. `jetpack_missions_test.cpp` covers run boundaries, three-slot replacement, cumulative input replay, restored state, purchases before a run, all criteria and all rank payouts. `jetpack_profile_test.cpp` covers the 128-byte save format, every-byte corruption, null inputs, wallet checkpoints, upgrades, inventories, SAM persistence and mission payout idempotency. These checks run with AddressSanitizer and UndefinedBehaviorSanitizer.

The deterministic 20km policy fixture produces 696 content slots, 31 powerups, 35 vehicle boxes, 26 spin tokens and 66 SAM letters. That fixture makes actor slots immediately available and does not model player collection. Separate core checks verify physical box encounter placement, upgrade effects, cash-in choices, repeated revives and practice-to-ranked isolation. Both private header variants also passed 9408 independent profile-weight checks each.
