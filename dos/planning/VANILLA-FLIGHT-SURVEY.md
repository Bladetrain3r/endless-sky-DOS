# Vanilla Endless Sky flight/world feature survey (pinned 0.11.4)

Reviewed by root 2026-09-30; detailed source audit supporting [the beta checklist](../VANILLA-PARITY.md).
Paths below are relative to the repository root; line references belong to the pinned source.

Scope: coverage inventory for a feature-parity beta roadmap, based on upstream
revision `061a9461a93898fb691504536589d1dcddc5d79b`. This is not an effort estimate,
priority order, or completion percentage. “DOS today” means the qualified narrow
implementation recorded in `dos/HANDOVER.md`, `dos/flight/NAVIGATION.md`, and
`dos/flight/ARENA.md`; it must not be read as qualification of the wider native
subsystem.

## Current DOS baseline to preserve

- **Qualified kernel:** stock Sparrow forward/turn/coast motion, shared movement /
  blaster energy and heat, shield recharge, overheat/recovery, disabled drift,
  ordinary bolt motion and swept sprite-mask hits. Evidence:
  `dos/motion/README.md:7-49`, `dos/propulsion/README.md:1-23`,
  `dos/resources/README.md:3-9`, `dos/collision/README.md:1-58`.
- **Qualified encounter:** two Star Barge trainers, target selection/HUD/radar,
  independent resource/shot pools, predictive pursuit and destruction. It has
  fixed hostility and explicitly lacks general factions, ship collisions, loot,
  landing, and complete AI (`dos/flight/ARENA.md:41-71`).
- **Qualified navigation slice:** Earth/Luna selection, physical Sparrow approach,
  native strict speed/radius/permission gates, fade, frozen dock, port restoration,
  and takeoff (`dos/flight/NAVIGATION.md:30-63`). It has no fuel/cargo/crew/calendar,
  dynamic politics, inter-system travel, or commerce (`dos/flight/NAVIGATION.md:52-56,81-85`).
- **Deliberate departures:** procedural tracers/explosions, indexed rendering,
  trainer RNG/loadouts, silent runtime, and fixed pools. Retain these as explicit
  substitutions unless a later parity decision replaces them.

## Coverage groups and candidate acceptance scenarios

### 1. Flight step, action ordering, and multi-entity lifecycle

**Native surface.** The engine updates ship caches, movement/events, boarding,
fighter launch, normal fire, then registers anti-missile and tractor systems in
`Engine::MoveShip` (`source/Engine.cpp:1904-1997`). Collision sets contain all
fully present ships (`source/Engine.cpp:2002-2011`); newly created ships,
projectiles, visuals, flotsam, weather, scanning and removals are coordinated by
the main step (`source/Engine.cpp:1607`, `source/Engine.cpp:1731`).

**DOS today.** The arena has a deliberately fixed three-ship ordering and fixed
shot pools (`dos/flight/ARENA.md:48-63`), not a general entity lifecycle.

**Acceptance candidate.** A deterministic mixed scene with player, escorts,
carrier/fighter, NPCs, projectile, flotsam and weather reproduces native per-tick
event ordering: same-tick disable suppresses new fire, existing shots survive
owner death, spawned/removed entities become visible at the same boundary, and
results do not depend on render cadence.

### 2. Ship movement, control authority, and resource competition

**Native surface.** Movement includes fractional turning/thrust based on available
resources, forward/reverse capability, drag and `STOP`, afterburners, slowness,
under-crewed pilot error, and disabled/energy-starved drift
(`Ship::DoMovement`, `source/Ship.cpp:5494-5599`; `Ship::StepPilot`,
`source/Ship.cpp:5462-5487`). Generation prioritizes own hull/shields, carried
craft, then energy/fuel transfer; it also handles delayed repair, disabled
recovery, solar/ramscoop input, fuel-to-energy conversion and active cooling
(`Ship::DoGeneration`, `source/Ship.cpp:4929-5130`).

**DOS today.** Stock Sparrow forward motion/turn/coast and a bounded resource
order are qualified; reverse, afterburner, crew, hull repair, fuel, solar and
carried-craft budgets are absent.

**Acceptance candidate.** Trace representative forward/reverse/afterburner,
under-crewed, slowness, low-energy, active-cooling and disabled-recovery ships
through input changes; compare pose and every resource/status level to native,
including exact threshold frames and repair priority with a docked fighter.

### 3. Player targeting, manual fire, secondary weapons, and autopilot

**Native surface.** The command set includes mouse/turret aim, primary/secondary
fire and selection, land/board/hail/scan/jump, asteroid targeting, deploy,
afterburner, cloak, autosteer and stop (`source/Command.cpp:49-95`). Autopilot
accumulates and cancels jump/stop/land/board commands on conflicting input
(`AI::UpdateKeys`, `source/AI.cpp:552-576`). Player control also includes minable
targeting and assisted steering (`AI::MovePlayer`, `source/AI.cpp:4396-5117`).

**DOS today.** Heading-only primary fire, ship cycling/nearest selection, manual
turn/thrust and landing-approach cancellation exist. Selection does not aim guns
(`dos/flight/ARENA.md:41-46`).

**Acceptance candidate.** In one input replay, acquire ship and asteroid targets,
cycle/fire secondaries, override turret aim, stop/autosteer, start each autopilot
action and cancel it with native-equivalent conflicting controls; assert command,
target and projectile traces as well as visible messages.

### 4. Armament, hardpoints, ammunition, and projectile families

**Native surface.** Guns/turrets occupy compatible hardpoints and support
streamed versus clustered timing, bursts, reload and ammunition
(`Armament::Add/Fire/Step`, `source/Armament.cpp:48-117,238-299`). Weapon data
supports safe, phasing, fused/gravitational, selective collision, homing with
blindspot/throttle/leading, submunitions with death triggers, penetration,
anti-missile, tractor beams, inaccuracy distributions and proximity trigger /
blast radii (`Weapon::Load`, `source/Weapon.cpp:38-190,218-250`). Homing combines
tracking, optical, infrared/heat, and radar/jamming locks and can become confused
(`source/Projectile.cpp:430-520`).

**DOS today.** One ordinary Energy Blaster-style bolt and an artificial hostile
turret are qualified; no hardpoint arcs, ammo, secondary selection, homing,
submunitions, blast, penetration, anti-missile or tractor behavior.

**Acceptance candidate.** A weapon matrix covers fixed guns and turrets,
stream/cluster bursts, finite ammo/empty fire, each homing sensor and loss of lock,
proximity blast with safe/friendly targets, phasing, penetration, submunition
spawn causes, anti-missile interception and tractor activation; compare projectile
state, ammo/resources, first collision and damage events to native.

**Terminology note.** The audited source has asteroid **mining** as a first-class
activity. It has no distinct deployable `Mine` entity/class in `source/`; mine-like
ordnance is representable through ordinary projectile attributes such as fused,
low/zero velocity, trigger radius and blast radius. Do not claim a separate
“space mines” feature without a stock data fixture demonstrating it.

### 5. Collision, damage, status effects, disable, and destruction

**Native surface.** Projectile collision distinguishes ships, decorative asteroids
and minables; trigger radii, phasing, penetration, blast area, safe friendly-fire
and anti-missile resolution are handled in `Engine::FindCollisions`
(`source/Engine.cpp:2420-2570`). Damage covers shield/hull plus energy, heat, fuel,
relative damage, piercing/permeability, force, prospecting, and eight persistent
channels: discharge, corrosion, ionization, burning, leakage, slowness,
scrambling, disruption (`DamageProfile::CalculateDamage`,
`source/DamageProfile.cpp:69-165`). Statuses tick and decay/resist with resource
costs (`Entity::DoStatusEffects`, `source/Entity.cpp:384-435`); slowness changes
motion, scrambling jams weapons/systems, ionization can prevent energy-using
actions, disruption weakens shields (`source/Ship.cpp:150-157,2709-2835,2904-2917,5500-5569`).
Destruction is staged, drops probabilistic cargo/outfit/ammo flotsam, and kills
unlaunched carried ships (`Ship::StepDestroyed`, `source/Ship.cpp:4757-4839`).

**DOS today.** Shield/hull damage, recharge, threshold disable/destroy, drift and
simple hiding/freezing exist in trainer form. Persistent statuses, force, area
damage, stochastic debris/flotsam and native destruction staging do not.

**Acceptance candidate.** For each instantaneous and persistent damage channel,
hit shielded/unshielded/protected targets and trace levels through decay and
resistance exhaustion. Add cases for piercing/permeability, force, blast falloff,
friendly-safe rules, overheat disable/recovery, hull disable versus destruction,
and seeded destruction drops including a carrier with occupied bays.

### 6. Cloaking, detection, and action restrictions

**Native surface.** Cloaked ships normally cease to be targetable
(`Ship::IsTargetable`, `source/Ship.cpp:2889-2892`). Cloak state can consume fuel,
alter shield/hull recovery and protection, phase projectiles, and separately
permit/forbid afterburning, boarding, communication, firing, pickup and scanning
(`Ship::CannotAct`, `source/Ship.cpp:2973-3002`; `source/Ship.cpp:3571-3617,4553-4619`).

**DOS today.** No cloak/detection model.

**Acceptance candidate.** Trace cloak entry/exit and fuel exhaustion; verify target
loss/reacquisition, each per-action permission, cloaked repair/permeability, full
and probabilistic phasing, AI target behavior, radar/HUD visibility, and a silent
jump where fitted.

### 7. Combat AI and personality behaviors

**Native surface.** AI chooses targets using governments, strengths, events,
personality and orders (`AI::FindTarget`, `source/AI.cpp:1554-1814`). Combat
supports ramming, artillery/range keeping, safe-distance handling, interception,
turret aiming and opportunistic/secondary fire (`AI::Attack/AimToAttack/
MoveToAttack`, `source/AI.cpp:2934-3057`; `AI::AutoFire`,
`source/AI.cpp:4084-4357`). Noncombat behaviors include appeasing, swarming,
surveillance, mining, harvesting, cloak, patrol, scatter and secretive movement
(`source/AI.cpp:3089-3667`), as well as rescue/refuel and independent travel
(`source/AI.cpp:1403-1535,1999-2403`).

**DOS today.** One Barge uses a bounded `MoveToAttack`/lead slice, without tactics,
avoidance, faction choice or the full behavior tree (`dos/HANDOVER.md`).

**Acceptance candidate.** Seeded scenarios exercise at least one ship for every
stock personality branch: target choice, flee/appease, disable/plunder, ramming,
artillery spacing, swarm/keep-station, surveillance/scan, mine/harvest,
patrol/scatter/secretive, rescue/refuel and autonomous land/jump. Compare commands,
targets and salient outcomes over fixed traces, not only final survival.

### 8. Fleet selection, orders, formations, and coordinated jumps

**Native surface.** Player fleet commands include fight, hold fire, gather,
hold position, harvest, scan and ammo policy (`source/Command.cpp:81-88`). The
order model adds move-to, keep-station, finish-off and mine; only compatible orders
coexist, invalid targets clear, mine transitions to harvest, and displaced holding
ships return to their point (`source/orders/Orders.h:33-53`;
`source/orders/OrderSet.cpp:45-66,134-205`). Formations cycle patterns and imply
gather (`AI::IssueFormationChange`, `source/AI.cpp:449-511`). Cross-system move
orders route independently; parents wait for eligible escorts before jumping
(`AI::FollowOrders`, `source/AI.cpp:1846-1954`; `AI::MoveIndependent`,
`source/AI.cpp:2187-2201`).

**DOS today.** Multiple hostiles exist, but no player escorts, fleet selection,
orders, formations, ammo policy or jump coordination.

**Acceptance candidate.** Select individual ships/groups, toggle every order,
verify compatibility/toggle rules, formation slots, hold-position recovery,
attack→finish-off, mine→harvest, completed scan cleanup, ammo policy, and a
multi-jump move where a parent waits, a carrier accounts for docked craft, a
stranded escort reports correctly, and the fleet eventually reunites.

### 9. Carriers, fighters, and drones

**Native surface.** Bays are typed; carriable ships dock through boarding and can
transfer collected cargo/ammo to the carrier (`Ship::CanCarry/Carry`,
`source/Ship.cpp:3850-3971`). Launch checks deploy state, landing/jump/cloak rules,
restocks ammo/fuel, places craft at bay geometry, and may eject/destroy craft when
the carrier dies (`Ship::Launch`, `source/Ship.cpp:2321-2397`). Docked craft run
generation; carriers prioritize their repair and transfer excess resources
(`source/Ship.cpp:4929-5019`). AI carriers deploy in combat
(`source/AI.cpp:2934-2941`).

**DOS today.** No bays or carried craft.

**Acceptance candidate.** A carrier with mixed bay types deploys only compatible,
ready craft; craft fight, harvest, return, transfer cargo/ammo, refuel/repair and
redeploy. Repeat while landing, jumping and cloaked with and without permission;
destroy the carrier during launch and while occupied to match seeded native eject /
loss outcomes.

### 10. Boarding, rescue, plunder, crew combat, and capture

**Native surface.** Boarding requires close position/relative speed and a valid
target; hostile targets must be disabled, fighters instead return to carriers,
and cloaking/self-destruct can prevent completion (`Ship::StepTargeting`,
`source/Ship.cpp:5603-5667`). Friendly boarding repairs a disabled ship and can
transfer fuel/energy; hostile boarding siphons fuel/energy and NPCs can auto-plunder
(`Ship::Board`, `source/Ship.cpp:2401-2513`). The player panel values and sorts
cargo/outfits, respects unplunderable items/capacity, can install stolen ammo,
resolves crew casualties/capture and enforces fleet capacity
(`source/BoardingPanel.cpp:54-119,269-537,594-784`;
`source/CaptureOdds.cpp:30-189`). Boarding can also trigger mission events before
plunder UI (`source/MainPanel.cpp:631-667`).

**DOS today.** No boarding, crew, cargo, capture, assistance or mission hook.

**Acceptance candidate.** Board a friendly disabled ship needing fuel/energy;
board an enemy with mixed cargo/outfits; test full hold, ammo installation,
unplunderable gear, self-destruct, cloak restriction, crew attack/defend casualties,
successful and failed capture, fleet-cap rejection, fighter recovery, and a
boarding mission that suppresses or proceeds to plunder exactly as native.

### 11. Scanning, hailing, provocation, fines, and faction response

**Native surface.** Cargo and outfit scans have distinct range/power/speed,
distance falloff, target-size/opacity cost, progress sharing across player ships,
inscrutable targets and silent scans; starting or completing a scan can provoke
governments (`Ship::Scan`, `source/Ship.cpp:2518-2686`). Scan results become ship
events after movement (`Engine::DoScanning`, `source/Engine.cpp:2790-2800`). Hails
are constrained by landing/jumping/cloak/translation and can address ships or
planets (`source/MainPanel.cpp:431-478`). Politics tracks offense, hostility,
bribes, landing/service clearance, reputation and fines
(`source/Politics.cpp:60-247,409-434`).

**DOS today.** Initial landing permission is snapshot data only; no dynamic
government, scan, hail, offense or fine state.

**Acceptance candidate.** Scan targets at multiple ranges/sizes with one and
several ships, break/reacquire target, confirm separate cargo/outfit completion,
opacity/inscrutable/silent outcomes and provocation. Hail eligible/ineligible
ships and planets, then commit an offense and verify reputation, hostility,
landing/service access, bribe and fine consequences persist across travel/save.

### 12. Landing, takeoff, wormholes, and port-state boundary

**Native surface.** Landing needs permission, operational state, speed `< 1`, and
position strictly within object radius (`Ship::CanLand`, `source/Ship.cpp:2953-2968`).
Landing fades toward the body; ordinary NPCs can disappear after escorts land,
spaceports refuel ships during departure, and wormholes relocate immediately
(`Ship::DoLandingLogic`, `source/Ship.cpp:5358-5438`). Planet accessibility can
depend on ship attributes, conditions, destination accessibility, reputation,
bribes and service clearance (`source/Planet.cpp:729-826`).

**DOS today.** The physical Earth/Luna flight/dock boundary is strongly qualified,
but only two static destinations and restoration/description display exist.

**Acceptance candidate.** Generalize the current test across planet/station sizes,
restricted attributes, hostile/reputation/bribe states, inhabited/uninhabited ports,
NPC/escort landing, interruption, partial refuel, and a bidirectional wormhole;
verify campaign date/state and service availability on both sides of dock.

### 13. Hyperdrive, jump drive, routes, fuel, and arrival geometry

**Native surface.** Navigation calibrates hyperdrive/scram/jump-drive availability,
fuel, range, speed and mass costs and chooses the cheapest valid method
(`ShipJumpNavigation::Calibrate/GetCheapestJumpType/CanJump`,
`source/ShipJumpNavigation.cpp:31-186`). Readiness checks disable/wait state,
target restrictions, fuel, departure distance, speed and scram thresholds
(`Ship::IsReadyToJump`, `source/Ship.cpp:3087-3144`). Hyperspace and jump drive use
different departure/arrival geometry/effects while preserving escort offsets
(`Ship::DoHyperspaceLogic`, `source/Ship.cpp:5203-5353`). Routes expose path, days,
fuel and per-leg costs (`source/RoutePlan.cpp:25-118`).

**DOS today.** No fuel or inter-system travel; Earth/Luna movement is ordinary
in-system flight.

**Acceptance candidate.** Travel a route containing linked hyperdrive legs,
range-based jump-drive legs and a wormhole; compare selected drive, mass-adjusted
fuel, readiness rejection reasons, scram-speed behavior, departure/arrival pose,
escort offsets/waiting, route days/fuel, silent/custom effects, stranded state,
and refuel diversion to native.

### 14. Exploration knowledge and galaxy-map state

**Native surface.** Systems may be hidden, shrouded or inaccessible and expose
visible versus linked/range neighbors (`source/System.cpp:797-908`). Player state
separately tracks seen, viewable, named and visited systems/planets, map radius,
mapped minables, and travel plan (`source/PlayerInfo.cpp:3035-3281`). Wormholes
have mappability and generated links (`source/Wormhole.cpp:140-227`).

**DOS today.** The packed world contains 694 systems/619 planets, but runtime flight
is bounded to Sol and has no map knowledge or save-format qualification.

**Acceptance candidate.** Starting from a fresh pilot, verify map reveal/name/visit
transitions for ordinary, hidden, shrouded and inaccessible systems, mapped and
unmapped wormholes, minable mapping, route selection and clearing, then save/reload
without leaking unknown data into map, targeting or routing UI.

### 15. Stellar world simulation, environmental resources, and hazards

**Native surface.** Parent/child stellar objects orbit by game date
(`System::SetDate`, `source/System.cpp:913-935`). Stars drive distance-scaled
ramscoop fuel, solar energy and heat (`System::GetSolarGeneration`,
`source/System.cpp:822-843`). Systems define asteroid belts, arrival/departure
distances, invisible AI fence, fleet events, hazards and danger
(`source/System.cpp:849-897,969-1185`). Hazards have periodic/random duration,
strength, radial or system-wide reach and weapon damage (`source/Hazard.cpp:27-70`;
`Engine::GenerateWeather/DoWeather`, `source/Engine.cpp:2103-2123,2622-2651`).

**DOS today.** Epoch-zero Sol object geometry is packed/displayed; no calendar
orbits, solar collection, hazards/weather or system population dynamics.

**Acceptance candidate.** Advance dates across a nested orbit, verify positions and
landing targets; fly the same collector ship at several stellar distances and
compare fuel/energy/heat; run seeded local and system-wide hazards through visual,
damage/status and expiry phases; check AI stays inside the invisible fence unless
its personality/target permits otherwise.

### 16. Asteroids, mining, prospecting, salvage, and tractor collection

**Native surface.** Decorative asteroids and damageable minables coexist and both
can intercept projectiles (`AsteroidField::Step/Collide*`,
`source/AsteroidField.cpp:87-166`). Minables follow stable eccentric orbits,
take normal/status/heat damage, and drop payloads probabilistically; prospecting
improves drop odds (`Minable::Place/Move`, `source/Minable.cpp:160-294`). Flotsam
has finite lifetime, drag, mass/capacity-aware partial transfer and source ownership
rules (`source/Flotsam.cpp:33-47,101-134,139-211`). Collection honors player
preferences and can use competing tractor beams (`Engine::DoCollection`,
`source/Engine.cpp:2656-2785`). AI supports mining and harvesting
(`AI::DoMining/DoHarvesting`, `source/AI.cpp:3316-3433`).

**DOS today.** No asteroids/minables, cargo hold, flotsam, mining AI, collection
preference or tractor beams.

**Acceptance candidate.** Target and mine a seeded minable among decorative
asteroids; verify occlusion, orbit/status/heat, prospecting-adjusted drops and
mine→harvest order. Collect partial stacks with flagship/escort preference modes,
source-government exclusions and insufficient capacity; expire unattended salvage
and resolve two tractor ships pulling the same box.

### 17. Ambient traffic, fleets, persons, governments, and encounter persistence

**Native surface.** Systems probabilistically spawn data-defined fleets while
limiting reinforcements when allies heavily outnumber enemies
(`Engine::SpawnFleets`, `source/Engine.cpp:2016-2047`). Named persons spawn by
frequency with shared origin and parent hierarchy (`Engine::SpawnPersons`,
`source/Engine.cpp:2052-2098`). Fleet definitions choose variants, government,
formation, cargo, fighter parents and jump/planet entry
(`Fleet::Enter/Place/Instantiate`, `source/Fleet.cpp:190-549`). Special/mission
ships persist across systems while ordinary distant ships are forgotten
(`Ship::StepFlags`, `source/Ship.cpp:4733-4752`).

**DOS today.** Fixed trainer opponents only; no ambient spawning, governments,
data-defined variants, mission persons or persistence/forgetting.

**Acceptance candidate.** On a fixed RNG seed, enter systems with several fleet
tables and persons; compare spawn timing/variant/cargo, carrier assignment,
government hostility, arrival source and reinforcement suppression. Follow a
special ship across a jump while ordinary traffic ages out, then save/reload and
verify only intended persistent actors remain.

## Cross-cutting beta gate

Each group should have (1) a native oracle/trace using the pinned source and stock
data, (2) a DOS deterministic comparison with stated numeric tolerances, and (3)
an end-to-end DOSBox gameplay scenario that exercises interaction with at least
one previously integrated group. Data loading, save persistence, rendering/HUD,
input edges, 16 MiB memory and fixed-cycle timing are separate evidence dimensions;
a helper match alone does not establish vanilla feature coverage.
