# Bounded Sol landing approach

`navigation_approach()` translates upstream `AI::MoveToPlanet` into a small C
helper for a stationary stellar target and the stock, fully crewed Sparrow.
Pass the selected object's world center and native `StellarObject::Radius()`.
The caller owns target selection, permission checks, LAND command, landing
state, resources, and motion stepping. When center distance is **strictly**
below radius and ship speed is **strictly** below 1 world unit/tick, this helper
returns a zero command. Native `Ship::CanLand()` applies the same inequalities,
but also rejects disabled or destroyed ships, WAIT, and denied planets.

The helper reproduces native `AI::StoppingPoint` without reverse thrust,
`AI::MoveTo` without cruise speed, and the already qualified
`pursuit_turn_toward`. It derives max velocity as acceleration / drag for this
stock configuration, then applies the native 0.99 margin. No altered upstream
objects or data are needed.

Run `python3 dos/navigation/check.py`. The native oracle links the unchanged
game objects via the existing `.work/native/build` linker seam. It exports the
stock Sparrow getter values into `.work/navigation/APPROACH.DAT` (same 48-byte
`ESPURS1` profile consumed by `pursuit_load`), command cases into
`.work/navigation/commands.csv`, and a current-condition planet snapshot into
`.work/navigation/PLANETS.json`, including resolved native planet descriptions.
The gate runs a C comparison under native
ASan/UBSan and in headless DOSBox at the project's 16 MiB/20k-cycle setting.
Both passed 20 cases with exact flags and zero observed turn error. Evidence:
`dos/reports/navigation-approach-equivalence.json`.

The planet snapshot uses a player-owned stock Sparrow in Sol after data load
and `Politics::Reset`, which applies each government's initial player reputation.
Earth and Luna both permit landing and services in that initial state. These
flags are a snapshot of that state, not a permanent property of the planets.
Descriptions come from `Planet::Description().ToString()`, so the game's
conditional text is resolved for the loaded starting conditions. The runtime should
require both `can_use_services` and the corresponding recharge flag for port
recharge. Native `Ship::Recharge` also fills shield/hull/energy through fitted
regeneration/repair/generation even without matching port flags and resets heat
to `IdleHeat` (source/Ship.cpp:3262). A simple dock screen can expose this
limited service status without claiming trade, jobs, missions, outfitters,
shipyards, cargo handling, or full takeoff economy.

Native landing starts when LAND and `CanLand` both hold. On the next movement
tick, `DoLandingLogic` takes over, suppresses ordinary acceleration, draws the
ship toward the stellar center at 3% of the remaining gap each tick, and
reduces ship zoom by the positive `landing speed` attribute or 0.02f fallback.
Ship generation still runs before landing logic on every fade tick until the
ship is destroyed. At zero zoom, the flagship enters the planet screen. Takeoff places the ship
at a random angle and radial fraction within the object's radius, initial
velocity one unit/tick outward; `Ship::Place` resets heat/status and grows zoom
from 0. Full campaign landing and takeoff also involve player, fleet, cargo,
missions, and economics and are beyond this trainer slice.
