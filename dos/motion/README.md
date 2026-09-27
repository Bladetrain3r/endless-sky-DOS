# Regular-flight equivalence slice

The stationary and moving-camera renderer passed the user's visual check on
2026-09-27. This next slice qualifies ship movement separately before replacing
the renderer's scripted traffic. The scripted watch mode remains available alongside the controllable scene.

`motion.c` implements a deliberately bounded part of upstream `Ship::DoMovement`
and the subsequent position update: healthy, fully crewed ships with sufficient
resources, ordinary forward engines, turn, coast, reverse capability and STOP.
No AI, resource accounting, afterburner, disability, slowness, collision, landing,
hyperspace, boarding or external acceleration is implemented here. Those require
their own contracts before this can be called a port of complete ship behavior.

The physics clock remains 60 Hz. Angle increments round onto the same 65,536
headings as upstream; the 16 rendered sprite headings do not constrain physics.
The initial implementation calculates sine/cosine on demand, avoiding the native
1 MiB unit-vector table; its measured DOS cost is recorded below.

Important rules to retain: healthy ships coast without drag; BACK without reverse
engines does not brake; acceleration-weighted drag is directional; STOP can
cancel only the facing-normal velocity under its precise overshoot condition.
The turn happens before thrust, then velocity updates before position.

Acceptance limits, set before running the comparison: each trajectory must keep
position error below 1e-7 world units and velocity error below 1e-9 world units per
tick, with exact quantized headings. No tolerance for different stop/coast/back
decisions. Native and DOS floating-point need not be bit-identical. Run cumulative
trajectories from their initial states, not independently reseeded single steps.


## Qualified checkpoint

`python3 dos/motion/check.py` builds the native reference harness, runs the
portable implementation under native ASan/UBSan, then cross-compiles and runs it
in headless DOSBox. `--reuse-oracle` uses the existing hash-checked traces. All
builds/tests run in the cached Docker images; source and native baseline remain
unchanged. Evidence: `../reports/motion-equivalence.json`.

The oracle links unchanged original objects, replacing only the program entry
point. It loads the actual fitted Sparrow and Star Barge through native game data,
then calls public `Ship::Move`. Six scenarios per ship cover forward, coasting,
turning, absent reverse engines, STOP and mixed commands with negative/fractional
turns across angle wrap. A thirteenth adds a native reverse-thruster outfit to a
Sparrow using the public API. A fourteenth replays the flight sandbox's held-key route. Each scenario records its initial state and 900
steps (15 simulated seconds). Crew/resources are checked, and the STOP case
explicitly verifies preserved lateral drift. The standalone harness stops before
native off-screen forgetting can remove the actor. It does not run Engine/AI.

All 12,600 cumulative steps pass. Native results are identical; DOS maximum
coordinate error is 5.46e-12 world units and velocity error 1.42e-14 per tick.
Quantized headings match exactly. Every one of the 65,536 direction vectors also
passes; DOS maximum component error is 1.11e-16. Boundary checks cover conflicting
commands, absent reverse engines, coasting, STOP and half-step angle rounding.
The angle-vector acceptance limit was 1e-14 per component.

At fixed20,000 DOSBox cycles,16MiB/no swap, 64 synthetic actors run 600 ticks in
about884ms: **1.47ms per64-ship simulation tick**. Only motion arithmetic is timed;
trace parsing, graphics, resource accounting, AI and collision are excluded.
The64 motion states occupy2,304 stack bytes in DOS. The kernel allocates no heap
and does not retain upstream's1MiB angle table; this is not a whole-game RAM claim.
No MMX/SSE compiler dependency; bundled libraries are not exhaustively audited.

The [controllable flight scene](../flight/README.md) now uses these resolved
Sparrow parameters and preserves this reference suite and camera/raster tests. Rendering16 sprite
headings is still a separate visible limitation; real-valued movement cannot by
itself improve those baked rotations. Scripted watch mode remains available for
comparison. Native resource accounting, collision and AI are separate future work.
