# What counts as equivalent behavior

The operator permits cheaper replacement logic, including new math and data
structures, with no MMX/SSE dependency. This is the working verification contract,
not a claim that the DOS port already meets it.

Preserve rules and content: thrust/turn/braking at a given simulation time,
projectile lifetimes and hit geometry, damage/outfit formulas, ship capabilities,
mission conditions and effects, trade transactions and persistent identity. A
palette, reduced decorative particles or another draw algorithm can differ;
changing what a ship can hit or which mission unlocks is a gameplay change.
The wishlist's soft concessions remain explicit choices with evidence, not
optimizations hidden inside benchmark results.

Before replacing a subsystem, identify observable inputs/outputs and a test case
where a plausible shortcut would be wrong. Examples: a projectile grazing an
original sprite mask; an angle crossing zero; two mission conditions reading the
same updated value; jumping back into a modified system; save/load after trading.
Use unit/property checks for math and state transitions, and scripted game scenes
for consequences. Retain upstream state/rule results as an oracle where possible.

For math changes, exhaustively compare bounded domains when cheap (all65,536
angle values, for example), and measure error in useful units such as world
position, angular error or time. Set the tolerance before accepting the candidate.
Then check the behavior at collision/threshold boundaries. A tiny numeric error
can still flip a discrete decision, so a small maximum error alone is insufficient.
Do not impose bitwise x87/SSE equality as a substitute for a gameplay contract.

Randomness deserves its own boundary. A fixed seed isn't a promise of the same
sequence across standard-library distributions, thread schedules or changed call
order. source/Random.cpp uses different storage/locking on Linux and non-Linux.
Choose a specified RNG/distribution or record/replay random draws for differential
scenes. Record actor iteration order and tick count when comparing those scenes.
Do not explain every mismatch away as random noise.

Benchmarks must state seed, starting state, simulated time, entity/projectile
counts and outcomes as well as frame time. Faster execution with fewer simulation
steps or missing actors is not equivalent optimization. Separate visual quality
concessions, intentional design changes and implementation-only improvements.

Start with upstream60Hzsimulation/30Hzdisplay. If that misses the budget, first
profile algorithms, lookup/data layout and rendering. A lower simulation rate
would need a separately verified time-step conversion through all affected rules.

Collision checkpoint: native drawing uses `Drawable::Width/Height` with a0.5factor;
Sprite::Width alone is not the physical-size contract. Preserve mask coordinates
and apply one consistent world-to-screen transform. Binary64 geometry transfer
matters at exact boundaries: see collision/README.md for the DOS decimal-transfer
disagreements and passing binary fixtures; do not silently relax discrete gates.

Ordinary-bolt checkpoint: `projectile/README.md` separates native constructor/Move
parity from the local firing-range composition tests. A correct Mask query alone
does not establish full Engine eligibility, damage ordering, beams or hardpoints.

Passive resource checkpoint: `resources/README.md` records energy/heat generation,
shot costs and overheat hysteresis against native Ship methods. The flight trainer
now couples propulsion through `propulsion/README.md`: native movement costs,
partial steering before thrust, shared weapon budget and overheat/energy drift.
This remains a bounded resource model, not full disabled-ship behavior. Explicit
stress presets alter supply/heat limits only for testing.

Resource boundary lesson: DOS decimal conversion changed simple fractions by a
tiny amount, leaving tiny power for thrust and triggering a finite drag correction. Use
exact binary64 profile/fixture transfer before judging arithmetic equivalence;
small input error does not imply a small gameplay error near branch boundaries.
