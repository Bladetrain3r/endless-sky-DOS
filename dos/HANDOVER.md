# Endless Sky DOS handover
Updated 2026-09-30. Two-opponent arena/HUD human accepted; several rounds, shield damage, victory.

## Current build
- Launch `python3 dos/flight/watch.py --arena`; Docker/X11,120seconds,800x600.
  W/Up thrust, A/D/arrows turn, Space fire; T cycle/N nearest target, Tab camera,
  R whole-encounter reset, H25% player shield drain, Esc/CtrlF9 exit.
- Arena:2 independent Barge trainers, own health/resources/pools,2second grace.
  Selection only changes HUD; shots hit nearest intersected living enemy.
  Player bars, selected health/range, radar, offscreen marker;6 visual buoys.
  Fixed open layout, no boundaries/collisions; disable/death/shot survival retained.
  flight/ARENA.md: ownership/order, controls, evidence and human checks.
- Earlier `--pilot --pursuit`, `--pilot --incoming-fire` and quiet `--pilot` remain.
- Sparrow has native movement, no reverse; healthy release coasts without drag.
  Shared energy/heat, fractional steering/thrust, overheat/recovery are human accepted.
  Shield recharge and incoming shield→hull→disable→destruction human accepted.
  Recharge .2 shield and .2 energy/tick, prior energy BEFORE generation/movement/fire.
  Generator1.9/tick > recharge .2: normal recharge does not visibly drain battery.
  --resource-stress energy: capacity40/generation.25; heat: max450/passiveheat0.
  Both are deliberate test presets, not stock loadouts.
- --incoming-fire target: scripted 8-second ellipse, artificial unlimited-power turret.
  Two-second grace, current-position aim (no lead), blaster speed/life/reload/damage.
  Orange/red enemy bolts; separate fixed16-shot hostile pool; native Sparrow mask.
  Target death stops new fire; shots already flying survive. No native turret tracking.
  Friendly hits resolve before turret firing; same-tick kill suppresses new shot.
- --pursuit: native Star Barge motion with AI::MoveToAttack/TurnToward helper.
  Starts650,140; current-position navigation; actual heading and physical movement.
  Turret uses native RendezvousTime lead, only finite intercepts within bolt lifetime.
  Own native Barge energy/heat/engine/recharge budget; no tactics/selection/avoidance.
  R restores enemy motion/pools/grace too. Dies or stops pursuit after player death.
- Hull <134 disables controls/fire/generation/recharge and applies native drift.
  Hull <0 destroys; trainer hides/freezes player and displays reset prompt.
  R restores both ships, resources, pools and grace. H four taps shortens hull test.
- Pursuit target defaults30shield/26hull/minimum9.516; native recharge .2/tick.
  --opponent-stock:600shield/1000hull/min366; --opponent-stress energy|heat.
  Enemy hull disable blocks control/fire/generation/repair, still drifts; R resets.
  --stationary-target and --invulnerable-target retain comparison modes.
- No ship collisions, landing, missions, saves, audio, repairs, boarding or loot.

## Current evidence
- Arena: native ASan/UBSan +DOS independent gates, nearest collision regardless of
  selection, same-tick kill/disable, owner-death bolts/shared player damage,
  target/reset and bounded pool/no dropped-shot resource charge pass.
- Renderer:88actual sprite/font cases under ASan/UBSan/leaks, edge/far positions,
  selection/death/camera, repeatability and no simulation mutation pass.
- 900tick arena route matches native player position/velocity within1e-7, heading
  exact.48hostile shots/46hits,0pooldrops. Counted heap2,901,342B (+2,100).
  29.361ms mean/34.344max uncapped frame; not whole-game FPS. Legacy per-layer
  timing fields unmeasured zeros in arena; whole draw measured. DOS capture reviewed.
- Arena actual keyboard pass:held T cycles once,N delivered,reset,camera,both owners
  firing,0discarded time. Quiet+pursuit application keyboard regressions pass.
- Existing opponent/power/threat native ASan+DOS and IRQ decoder/vector tests pass.
  Reports:flight-arena-tests.json,flight-arena-view-tests.json,
  flight-controls-arena.json,flight-6-ships-arena-profile.json under dos/reports.
- Previous native oracles retained:shield10cases423ticks; Barge430generation ticks;
  pursuit16commands/11intercepts, zero observed error. See module READMEs/reports.
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
- Human: several rounds won; HUD has key info, DOS sci-fi look accepted as baseline.
  Polish after playable systems/screen contents settle. Proposed next: navigation/landing.
- Powered single-opponent normal/heat/energy humanaccepted; stock HP unreported.
