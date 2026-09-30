# Endless Sky DOS handover
Updated 2026-09-30. Beta parity survey/roadmap/local queue added; landing human test pending.

## Current build
- Latest: `python3 dos/flight/watch.py --navigation`; Docker/X11,120seconds,800x600.
  Quiet Earth/Luna: P cycles destination, L approaches/lands; L at dock departs.
  W/Up thrust,A/D/arrows turn cancel approach; Space fire,Tab camera,R reset,H drain.
  Suggested human test: H,L Earth repairs; release/press L depart; P,L to Luna.
- Native-derived stock Sparrow braking/turning approach; strict radius/speed gates.
  Binary32 landing/takeoff fade, original descriptions/initial permissions/services.
  Dock restores hull/shield/energy, sets native IdleHeat; space sim pauses at dock.
  Held L cannot depart; flight controls need release after takeoff. Seeded placement.
  Only two epoch-zero destinations; no cargo/fuel/trade/outfitting/missions/saves yet.
  flight/NAVIGATION.md records ownership, native contracts, limitations and tests.
- Accepted arena remains `--arena`:2 independent Barge trainers,2second grace;
  Tcycle/Nnearest, selection only changes HUD, nearest intersected ship gets hit.
  Bars/radar/offscreen marker,6visualbuoys,no boundaries/collisions. See flight/ARENA.md.
- Earlier `--pilot --pursuit`, `--pilot --incoming-fire` and quiet `--pilot` remain.
  R whole-encounter reset, H25% player shield drain, Esc/CtrlF9 exit.
- Sparrow native movement/no reverse; healthy release coasts without drag.
  Shared propulsion/gun energy/heat, fractional steering/thrust, overheat/recovery.
  Recharge .2shield/.2energy per tick, uses prior energy BEFORE generation/motion/fire.
  Generator1.9/tick > recharge .2: normal recharge does not visibly drain battery.
  --resource-stress energy:capacity40/generation.25; heat:max450/passiveheat0.
  Deliberate test presets, not stock loadouts. Normal/heat/energy human accepted.
- --pursuit: native Star Barge motion/MoveToAttack/TurnToward/RendezvousTime lead.
  Own engine/energy/heat/recharge budget; no tactics/avoidance/full-AI parity claim.
  Default30shield/26hull/minimum9.516; --opponent-stock:600shield/1000hull/min366.
  --opponent-stress energy|heat; stock HP human test unreported.
- --incoming-fire: scripted ellipse, artificial unlimited-power turret, no lead.
  Fixed16-shot pools; native masks; friendly hits before hostile firing;
  owner death stops new fire, existing shots survive. R resets pools/grace/resources.
- Hull <134 disables Sparrow controls/fire/generation/recharge and applies drift.
  Hull <0 destroys; trainer hides/freezes player until R. H four taps aids hull test.
- Combat modes have no landing/port services; no ship collisions/audio/boarding/loot.

## Current evidence
- Navigation helper:20 native command cases, exact flags/zero turn error, native
  ASan/UBSan +DOS; initial Earth/Luna permissions require Politics::Reset.
- Native/DOS navigation state tests:strict gates,physical approaches,port services,
  dock freeze,held-L safety,departure release/reset,interruption,malformed profiles.
- Navigation renderer:88 actual sprite/font ASan/UBSan/leak cases; no state mutation.
- 900frame Earth→Luna route:2landings/2takeoffs,1682space+118dock ticks,0enemy fire.
  Counted heap2,901,550B;26.576ms avg/62.183max mixes flight/dock,not game FPS.
  Initial full dock text draw slower; subsequent frames cached. DOS captures reviewed.
- Actual final-executable keyboard:landing/held L,port restoration,departure/fire,
  approach cancellation; arena/quiet/pursuit regressions. IRQ decoder tests pass.
- Reports dos/reports/flight-navigation-{tests,view-tests}.json,
  flight-controls-navigation-{dock,launch}.json,flight-6-ships-navigation-profile.json,
  navigation-approach-equivalence.json. Profiles from dos/navigation/native.py.
- Prior arena gates,900tick player parity,renderer88cases and power/threat tests pass;
  see flight/ARENA.md and prior reports. Navigation does not requalify full native AI.
- Docker i386/noMMX/SSE,16MiB/20k/no swap; cycles are not MHz. Source/data unchanged.

## Earlier contracts and pointers
- dos/propulsion/: native Ship::Move17cases667steps, zero observed state error;
  generation→turn→thrust→gun; native overheat/NeedsEnergy drag. 900tick route matched.
- IMPORTANT: DOS decimal reader perturbed even .5/.25, changing thrust/drag branch.
  Exact binary64 runtime profiles/traces fixed transfer; no epsilon/snap gameplay.
  RESOURCE.DAT ESRES2 +7d; PROPULSE.DAT ESPROP2 +4d; SHIELD.DAT ESSHLD2 +7d+2u32.
- dos/motion/:14cases12600ticks/all65536directions. dos/collision/:11772queries.
  flight/ALIGNMENT.md: half-PNG world scale,64 baked headings; native masks unchanged.
- Projectile: life48→47moves, inherited velocity, native half-parent muzzle correction;
  post-move target sweep, birth-tick queries, fraction<1. Player spread uses seededLCG.
  White7pixel tracers and procedural target explosion are explicit visual departures.
- dos/world/:694systems/648042Bpack,619planets10commodities5518objects.
  Native all-orbits crashes; bounded Sol qualified; no save format qualification.
- Palette256=16UI+32gray+208learned/6bitDAC human accepted. Wishlist unchanged.
  Upstream061a9461a93898fb691504536589d1dcddc5d79b,0.11.4; Git, GPL3+/asset terms.
  Original source/data/images untouched. No bundled-library-wide ISA audit.
- Earlier preserved flight .work/flight/accepted-44aced341; --accepted uses0636c12dc.

## Next
- User: vanilla parity is beta; our flair follows toward1.0. dos/VANILLA-PARITY.md
  +ROADMAP.md +TICKETS.md; detailed source audits in dos/planning/,204-file inventory.
- ESDOS-001 awaits landing playtest; ESDOS-002 ready: persistent-pilot/save contract.
  ESDOS-003 durable pilot,004 trade,005 travel spec proposed; none started.
- BBS assessment ../../docs/BBS-TASK-BOARD-ASSESSMENT.md: aggregate first; no deployment.
- Arena humanaccepted:several rounds,damage/victory; HUD polish deferred until systems settle.
