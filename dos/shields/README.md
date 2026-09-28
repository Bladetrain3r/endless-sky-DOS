# Stock Sparrow shield regeneration

`shields.c` adds the player's own shield recharge to the shared `ResourceState`.
Call `shield_tick` **before** `resource_tick`, passing the ship's disabled state
from the previous tick. The native `Ship::DoGeneration` repairs shields before it
clamps energy, dissipates heat, updates overheat, and produces new energy. A ship
that recovers from overheat this tick cannot recharge shields until the next tick.
For the bounded full-hull, staffed Sparrow, prior `ResourceState.overheated` is
the corresponding disabled flag. With hull damage enabled, the caller also
passes the hull-disabled flag. `shield_reset` restores full shields and zero
delay. `shield_damage` implements a positive shield-only drain and the native
shield-delay choice; the trainer's H key can call it with 25% of capacity.

The stock loaded Sparrow profile is 1400 shield capacity, 0.2 shield/tick and
0.2 energy/full recharge tick, zero shield heat and zero delay. Its delayed values
equal its ordinary values. Shield recharge uses residual energy from the previous
tick, so movement and firing can defer it. `ResourceLevels::DoRepair` scales
recharge to affordable energy and remaining shield capacity. The native code
divides the energy cost by the recharge rate, but does **not** divide the shield
heat cost; the kernel retains this behavior. Synthetic fixtures exercise a
different delayed rate/cost and positive shield heat.

`python3 dos/shields/check.py` links against the cached unchanged native source objects in
Docker, substituting only the oracle main object, and compares 10 cases/423 ticks
with a portable ASan/UBSan checker and a 16 MiB, 20,000 cycle DOSBox run. The
observed maximum absolute shield/energy/heat error is zero; overheat and delay
flags match exactly. Cases include full, partial, depleted, energy-limited,
capacity boundary, overheat entry, recovery with prior disabled state, delayed
recharge, heat cost and shield damage. The synthetic damage fixture directly
sets the native shield level/delay using the `DoTakeDamage` selection rule; it
does not invoke the full native damage pipeline. Evidence is
`dos/reports/shield-equivalence.json`.

The native getter export is `.work/shields/SHIELD.DAT` (`ESSHIELD1` text).
`assets.py::stage(out)` verifies oracle, source and data provenance, then writes
`SHIELD.DAT` as `ESSHLD2\0`, seven little-endian binary64 values and two
little-endian unsigned delays. `shield_load` rejects malformed/nonfinite/out of
range values without changing its destination. This exact binary64 staging
also emits `PLAYER.DAT` (`ESPLAYER1` + two binary64 values) for loaded maximum
hull and minimum operating hull. Native assertions check no generation/repair
below that threshold and the strict equality boundary. Exact binary64 transfer
avoids DJGPP decimal-reader drift that previously changed propulsion behavior.

This is a stock Sparrow slice. The oracle rejects stock hull repair, fuel/hull
shield costs, negative heat cost, carried craft, cloak recharge multiplier,
active cooling, fuel conversion, disabled recovery and overheat hull damage.
The kernel does not claim parity for hull repair, carried craft, cloak/status
effects or the complete damage system. It shares energy and heat with the
existing resource/propulsion/gun order; it does not alter upstream source or data.
