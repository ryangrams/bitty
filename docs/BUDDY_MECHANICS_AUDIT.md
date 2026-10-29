# Buddy runner audit

The reference is the user's local Subway Surfers 3.33.0 APK. Its package identity, version, SHA-256 and decoded configuration receipt are retained privately. No reference models, textures, route graphs, scripts or serialized configuration are included in Bitty's source or firmware.

Static configuration establishes available settings and structures. It does not establish which live configuration, event or upgrade a particular play session uses. This audit keeps those distinctions in its private evidence ledger.

| Gap in the earlier Buddy build | Implemented change |
| --- | --- |
| Repeating six-obstacle sequence | Connected sections for slaloms, hurdles, low passages, crowds and mixed action rhythms |
| One coin per row | Ground trails, optional jump arcs, roll-gap trails and coin-rich bonus sections |
| Sparse reward feedback | Three run goals, increasing score multiplier, trick chains, pocket prizes and visible status |
| Similar defensive pickups | Animated magnet, temporary score boost and an authored balloon flight, alongside the existing board and snake |
| Character faces the player while running away | Rear-facing running, jumping, rolling and flight poses; face visible during intermissions |
| Controls lock out an early jump or a change of action | Brief jump buffering, dive-to-roll input and jump interruption of a roll |
| Saved neutral can be wrong at a new launch | Stable automatic tilt calibration on launch, with manual zero retained |
| Fixed row allocation can suppress content for a full interval | Reserve a complete row before advancing its schedule, then retry promptly when the pool is full |

The reference developer documents [distinct pickup effects](https://sybo.helpshift.com/hc/en/5-subway-surfers/faq/209-power-ups/) and [boards that protect the player and change movement](https://sybo.helpshift.com/hc/en/5-subway-surfers/faq/208-hoverboards-their-powers/). Those general ideas inform the review. Buddy's artwork, route recipes, score goals, durations, reward amounts and small-screen pacing remain authored adaptations.

Buddy keeps five ten-minute locations from its existing design. It has no advertising, purchases, reference-game account, imported character artwork or commercial progression data. Persistent currency, shops, elevated multi-floor courses, moving crowds and branching route geometry remain future work. Current rewards affect the run and its best-score chase.

Tests cover action timing, collisions, reachable safe paths, all locations and difficulties, deterministic reset, bounded pools, reward accounting, power transitions, clipping and clock isolation. The shared source suite uses sanitizers. A private browser playtest runs the same C++ engine and renderer. Physical tilt and completed LCD throughput still need the connected device.
