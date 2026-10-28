---
name: m5stick-game-fidelity
description: Audit a tiny-screen game port against a user-provided reference, including placement, pacing, vehicle physics, camera and bonuses. Use for measured fidelity work; do not substitute an inspired design for a requested faithful port.
---

# M5Stick game fidelity

Read the existing evidence ledger and distinguish configured values, recovered code behavior, observed gameplay and deliberate hardware adaptations. Record the source hash/version and units. Read [reference audits](references/audits.md) for the project-specific examples and failure patterns.

Build a gap list around player-visible behavior: spawning, order and frequency, placement, collisions, each vehicle, coin collection, death, transitions and bonuses. Trace the scheduler before adding isolated features. Wrong content spans or resource gating can make correct weights produce the wrong experience.

Compare at equivalent distance, speed, seed and game state where possible. Measure encounter distances and counts over many deterministic runs. Test pauses, menus, retry and vehicle transitions. Compare independent reference traces when available; testing one implementation against its own formulas cannot establish fidelity.

For emulation, compare full RAM and full-frame video with optimized paths. Use an independent emulator for suspected native behavior. Fingerprint cartridge-specific RAM integrations. Derive camera position from stable game coordinates, not animation ink or OAM visibility. Keep test-only scene injection out of production.

Import proprietary data from the user's local source into ignored outputs with receipts and validation. Public tests use synthetic fixtures. Keep original artwork optional and private; author replacements that remain legible at actual screen size. State any adaptation explicitly and measure its effect on pacing and playability.

Finish with evidence that identifies the build, input source, timing method, scenario, sample count and remaining gaps. Do not claim physical controller, visual or audio verification from synthetic input alone.
