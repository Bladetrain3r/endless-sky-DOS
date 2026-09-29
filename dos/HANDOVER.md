# Endless Sky DOS handover
Updated 2026-09-30. Powered opponent human accepted: normal/heat/energy rounds.

## Current build
- Launch `python3 dos/flight/watch.py --pilot --pursuit`.
  Docker/X11,120seconds. --incoming-fire alone keeps ellipse; omit both flags for quiet range.
  W/Up thrust, A/D/arrows turn, Space fire, Tab camera, R reset, Esc/CtrlF9 exit.
  H drains 25% player shields once per press (explicit trainer input).
- Sparrow has native movement, no reverse; healthy release coasts without drag.
  Shared energy/heat, fractional steering/thrust, overheat/recovery are human accepted.
  Shield recharge and incoming shield→hull→disable→destruction human accepted.
- Stock shield1400, hull300, hull-disable threshold134, all native getter-derived.
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
  Power, thermal state and health preset shown on enemy HUD;48tick death sparks.
  --stationary-target and --invulnerable-target retain comparison modes.
- No ship collisions, landing, missions, saves, audio, repairs, boarding or loot.
- Local checkpoint only; central HANDOVER has commit. No public push.

## Current evidence
- dos/shields/README.md: unchanged Ship::DoGeneration,10cases423ticks,
  zero observed shield/energy/heat error native ASan/UBSan and DOS; root reproduced.
- flight/test_threat.py: native ASan/UBSan +DOS direct/overflow/dodge/grace,
  inherited velocity, source-death shot, strict hull thresholds, reset, bad profile.
  Root independently reproduced; reports/flight-threat-tests.json.
- flight/test_power.py: recharge/generation/steering/thrust/fire order, heat recovery,
  persistent hull disable, drift, camera/reset. Native ASan/UBSan and DOS pass.
- Pursuit helper:16commands/11intercepts, zero observed error native ASan/UBSan
  and DOS; root reran. NaN/infinity classifications and malformed profiles checked.
- Opponent integration native ASan/UBSan +DOS: physical approach, bounded turns,
  postmove collision pose, death/reset, predictive aim/range gates all pass.
- Pursuit900tick route preserves accepted player motion;26shots22hits,0pooldrops.
  reports/flight-6-ships-pursuit-profile.json; root inspected capture.
  Counted heap2,899,242B (+224 vs pursuit); excludes code/stack/runtime/VRAM.
  23.126ms mean frame on route leaving Sol backdrop; not full-battle FPS.
- Final quiet/incoming/pursuit actual keyboard tests pass;0discarded sim time.
  Reports bind current executable/profiles. Default/stock/energy/heat app routes pass.
- opponent_profile/:430native generation ticks, zeroerror native/DOS, root verified.
  test_opponent.py: costs/repair order/overheat/disable/reset/presets ASan+DOS pass.
  All tests/builds Docker, i386/noMMX/SSE,16MiB/20k/no swap; cycles are not MHz.

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
- Human normal/heat/energy pass: low-energy fire slows/rotates; overheat halts motion.
  Stock-health human test unreported. Next: choose navigation/multiple-target slice.
