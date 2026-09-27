# Endless Sky DOS

Read Wishlist.md (operator brief), dos/HANDOVER.md and dos/README.md first.
Keep the upstream game as a behavioral reference; record deliberate departures.
Docker for builds/tests, headless DOSBox for unattended runs. Separate sim-time,
render time, guest RAM, host RAM, VRAM and disk measurements. DOSBox cycles are
not a calibrated hardware clock. Never label a graphics microbenchmark game FPS.

Put port investigations/tools under dos/ until a justified source boundary changes.
Retain upstream GPL notices and per-asset attribution. Do not overwrite the user
wishlist with inferred requirements. User delegates low-level design choices and authorizes logic replacement for
cheaper equivalent behavior. No MMX/SSE dependency; verify equivalence against
reference traces and gameplay outcomes, with explicit numerical tolerances.
Keep source modules near500lines where practical; preserve legibility over count.
Use bounded delegates for independent audits or clear implementation/test contracts;
verify their source claims and results before adoption. Keep evidence compact.
Builds and emulator output live in ignored .work/. No broad cleanup of other
projects or user-owned assets. Do not push a public fork without authorization.
