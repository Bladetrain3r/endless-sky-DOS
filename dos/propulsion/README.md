# Stock Sparrow propulsion resources

`propulsion_step()` applies the stock Sparrow's turning and forward thrust costs
to the shared `ResourceState`, then integrates velocity and position. Its caller
must run `resource_tick()` first and may attempt `resource_fire()` afterward.
The function does not perform passive generation or cooling. Turning spends its
fractional energy first; thrust receives the remaining budget. A fractional
thrust scales acceleration, while the ordinary drag coefficient stays intact.
BACK has no reverse engine and simultaneous FORWARD/BACK cancels. Overheat and
the native `NeedsEnergy()` passive subset cause drag-only drift, then position
integration; held thrust resumes when the condition clears. The energy threshold
is `<= 0.00390625` only when capacity is positive, the battery is below
capacity, net generation is nonpositive, and movement uses energy.

`native.py` links `oracle.cpp` against unchanged native objects and calls actual
`Ship::Move()`, then optionally `Ship::ExpendAmmo()` to test budget competition.
The reference is a full-health, fully crewed stock Sparrow in Sol, with
synthetic battery/generator/heat limits. It exports exact stock `cache.thrustCost`
and `cache.turnCost` getters to a raw text profile. `assets.py stage(out)` verifies
its source/data provenance and stages `PROPULSE.DAT` as `ESPROP2\0` followed by
four little-endian binary64 values: thrust energy, thrust heat, turn energy,
turn heat. The binary transfer is necessary because this DJGPP decimal parser
rounded `.5` and `.25` away from their exact binary64 representations.

From the checkout root, run `python3 dos/propulsion/check.py`. It uses Docker to
build a native ASan/UBSan checker and an i386 DOS checker, then replays the latter
under headless DOSBox with 16 MiB and fixed 20,000 cycles. The 17 native cases
cover stock supply, tiny batteries, both turn signs, forward/back cancellation,
coasting, overheat/recovery, exact energy threshold boundaries, zero capacity,
and optional competing shots. The trace transfers binary64 fields exactly.
The checker also rejects four malformed staged propulsion profiles without
changing the destination. Current result: 667 steps, zero observed position,
velocity, energy and heat error in both checkers; exact angle, firing and
overheat flags. Numeric acceptance limits are position `1e-8`, velocity
`1e-10`, energy/heat `1e-9`. See
[`propulsion-equivalence.json`](../reports/propulsion-equivalence.json).

This is a bounded propulsion/resource slice, not a whole-ship or whole-Engine
oracle. It excludes reverse engines, afterburners, repair, shields, status,
fuel, solar, active cooling, projectile cadence, and damage. The existing
motion kernel supplies the physical integration.
