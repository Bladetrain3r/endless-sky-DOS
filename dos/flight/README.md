# First DOS flight-scene prototype

The800×600,256-color DOS executable loads Sol from the compact world pack and
now offers a controllable stock Sparrow using native-matched movement. Earth,
Luna, the Falcon and scripted traffic provide reference points. This is a flight
sandbox with one training blaster and a destructible scripted target.
No ship collisions, landing, AI, missions or saves. Propulsion and firing now share an
energy/heat budget.64 baked sprite headings are
still an experimental rotation shortcut; simulation uses all65,536 headings.

User playtest2026-09-27: flight works and movement feels physically right;
operator notes limited familiarity with the original game. Sept28: user confirms
corrected smaller scale and much smoother turning. Subsequent Sept28 playtest: firing works, target hits look correct, and no
oddities observed while moving or firing; basic firing range accepted. Subsequent moving-range test also accepted:
stationary/moving fire works and leading is predictable on the steady route.
Sept28: user also confirms firing stops while overheated and resumes after cooling,
and energy recharge limits firing in the stress preset. The subsequent shared-power
playtest also passed: stock Sparrow limits are difficult to reach, while stress
presets visibly cut thrust and/or rotation when resources run out.

World sprites now use native half-PNG dimensions at one screen pixel per world
unit: `Drawable::Width/Height` supply the 0.5 factor to `DrawList::Push`.
Original collision masks and flight arithmetic are unchanged. The old full-size
build is preserved locally; `watch.py --pilot --accepted` runs it for comparison.
See [alignment qualification](ALIGNMENT.md) and [collision notes](../collision/README.md).

## Player shield recharge (2026-09-28)

The Sparrow now has its stock 1,400 shield points and D14-RN generator:
0.2 shield points per simulation tick (12/second), costing 0.2 energy per tick
while fully recharging. Recharge spends the previous tick's remaining energy,
before generator output, steering, thrust and firing. Overheating suspends it;
on the cooling-recovery tick movement can resume one tick before recharge.

Press **H** to remove 25% of maximum shields as an explicit trainer input.
Holding H drains once; release and press again for another pulse. No hull damage
is applied. The new shield readout shows recovery; one pulse takes roughly
29 seconds to refill with normal resources. **R** restores shields and resources
along with the rest of the trainer. Try the existing `--resource-stress energy`
and `--resource-stress heat` presets to observe competition and suspension.
The target retains its reduced, non-regenerating training health.

See [shield qualification](../shields/README.md) for native oracle coverage and
supported profile limits. User accepted shield recharge and overheat suspension. Normal generation exceeds
shield demand: 114 energy/second generated versus 12 spent at full recharge.
Even the energy stress preset supplies 15/second; movement/fire create competition.

## Optional incoming-fire trainer (2026-09-28)

```sh
python3 dos/flight/watch.py --pilot --incoming-fire
```

The training target fires orange/red bolts after a two-second grace period.
It aims at your current position; sustained movement lets you dodge. Destroying
it stops new shots, but shots already in flight remain live. Friendly and hostile
bolts use separate fixed 16-shot pools and collide with their respective target.
Add `--stationary-target` for an easier fixed comparison. Omit `--incoming-fire`
for the accepted quiet range. No extra health nerfs are applied to the player.

The loaded Sparrow has **1,400 shields and 300 hull**. Shields absorb shots and
recharge from the shared energy supply. Hull below the native **134** threshold
disables movement controls, firing, generation and recharge; momentum decays.
Hull strictly below zero destroys the ship. The trainer then hides/freezes the
player and displays a reset prompt. **R** restores both ships, shields, resources,
projectile pools and the grace timer. **H** remains the explicit shield-drain
control; four separate taps let you reach the hull test much sooner.

This is an artificial turret attached to the scripted target, with unlimited
ammunition/power, unrestricted aim and no native AI or turret-turn-rate claim.
It uses the loaded blaster speed, lifetime, reload and damage, but no inaccuracy;
the target's observed motion contributes inherited projectile velocity. Damage
uses the existing native-qualified shield/hull kernel and the player's post-move
Sparrow mask. Friendly shots resolve before the turret's firing decision: a
same-tick target kill prevents a new hostile shot. That ordering is a trainer
choice, not a claim of full native Engine combat parity. No boarding, repairs,
loot, explosion damage, player death persistence or enemy resource model yet.

Tests: `test_threat.py` (portable sanitizers + DOS); `test_power.py` (disabled
resource/repair/drift); `test_controls.py --incoming-fire` (actual keyboard);
`run.py --incoming-test --frames 600` (drained-shield stationary death route).
Evidence lives in `dos/reports/flight-threat-tests.json`, `flight-controls-incoming.json`
and `flight-6-ships-incoming-profile.json`. User accepted incoming fire through disable and destruction.

## Pursuit opponent (2026-09-28)

```sh
python3 dos/flight/watch.py --pilot --pursuit
```

This mode implies incoming fire and replaces the ellipse with a physically moving
Star Barge, initially at (650,140). It turns toward you and applies thrust using
the original `AI::MoveToAttack`/`TurnToward` rules for that fitted ship, through
the existing movement kernel. Its gun predicts your movement using the native
`AI::RendezvousTime` helper and fires only at finite intercepts within bolt lifetime.
Keep moving, change direction, and try flying past it; **R** resets the opponent,
player, shots and two-second grace period together. The opponent stops receiving
pursuit commands after either ship is destroyed.

The hull/sprite now turns with its simulated heading. Gun aiming is still an
artificial freely aiming training turret: the stock Barge's anti-missile turret
is not being portrayed as a blaster. Enemy power remains unlimited and target
health stays at the deliberately reduced 30 shields/26 hull. No target selection,
retreat, obstacle avoidance, personality or complete vanilla AI claim is made.
The native movement helper uses current target position; projectile leading is
a separate trainer rule using native interception math from the ship centres.

`--incoming-fire` alone retains the scripted turret/ellipse comparison; omitting
both flags retains the quiet range. `--stationary-target` conflicts with pursuit.
The native helper cases and exact binary64 fitted Barge profile are documented
in [pursuit qualification](../pursuit/README.md). `test_opponent.py` checks physical
approach, bounded turns, shared collision/render pose, lead/range limits and reset.
`run.py --pursuit-test --frames 450` also compares the player against the accepted
900-tick flight route while the Barge pursues. Human pursuit playtest pending.

## Corrected-scale checkpoint (2026-09-27)

Rotating hulls use64 resident headings (5.625 degrees apart), selected nearest the
simulation heading. Rotation is baked from original-resolution art before half-size
filtering; even canvas dimensions keep image and world centres consistent.
Scripted traffic still turns22.5 degrees once per second, now selecting every fourth
frame. Player rotation uses all64. No runtime sprite reloading is needed.

On the same360-frame moving-camera route at16MiB/fixed20,000cycles, corrected
scale reduces average frame time from30.186ms to21.189ms; maximum26.339ms.
This is a smaller, correctly scaled scene, **not** a faster raster algorithm or a
complete-game FPS claim. Decoded sprites846,472B; total counted heap2,847,714B
(including span metadata/world/frame/LUTs; excluding runtime/stack and480,000BVRAM).
More headings add spans despite smaller sprite pixel storage, so total heap stays
close to the prior2,824,046B. Scalar/span equality passes1,188 heading/edge cases;
three malformed sprites reject, including65frames. Camera checks pass2,880ticks
and63whole-frame comparisons. The900tick pilot replay still matches native motion.

Earlier timing sections below retain historical full-size-art measurements.

## Fly

```sh
python3 dos/flight/watch.py --pilot
```

The reference Docker/DOSBox opens a two-minute flight session. Controls:

| Action | Keys |
|---|---|
| Thrust | W / Up |
| Turn | A/D / Left/Right |
| Fire training blaster | Hold Space |
| Coast | Release thrust |
| Toggle following/fixed camera | Tab |
| Reset ship, camera and firing range | R |
| Exit | Escape; Ctrl+F9 exits DOSBox |

Turn and thrust can be held together. The stock Sparrow has no reverse thrusters,
so S/Down does not brake. To slow down manually, turn around and thrust against
your motion; autopilot braking is not implemented. The camera starts following
at the ship's centre; Tab freezes its current position. R is useful after flying
out of sight. `--seconds N` selects1–120seconds. Local DOSBox alternative:
`dosbox -conf .work/flight/pilot.conf`.

A labelled moving practice target starts ahead and follows an eight-second oval.
Add `--stationary-target` to `watch.py --pilot` for the old fixed range.
Hold Space to see bolts, hit
flashes and counters. The reduced training target has30 shields/26 hull, no regeneration, and takes
seven ordinary blaster hits to destroy. Shields/hull appear in the lower HUD.
Destruction removes it from collision and triggers a brief procedural explosion;
R restores it. Add `--invulnerable-target` for the previously accepted aiming range.
Background ships/planets are not hittable. This is one test blaster, not the stock Sparrow's beam weapons.
See [projectile qualification and deliberate limits](../projectile/README.md).

### Shared energy and heat

The lower HUD shows battery energy, heat as a percentage of the overheat limit,
and READY / RELOADING / GUN LOW ENERGY / OVERHEATED. Thrust, steering and
successful shots spend energy and add heat. Release the controls to recharge
and cool; R restores a full
battery and cold ship as well as the target. Native generation permits a small
amount of energy above battery capacity for the current tick.

The normal profile uses the stock Sparrow's supply, cooling and engine costs with
one training Energy Blaster. Its battery comfortably covers this short trainer
session. To deliberately exercise limits:

```sh
python3 dos/flight/watch.py --pilot --resource-stress energy
python3 dos/flight/watch.py --pilot --resource-stress heat
```

These are **deliberate trainer overrides**, labelled in the HUD. Energy stress
sets capacity40 and generation0.25/tick. Heat stress sets the heat limit450 and
removes generator heat, so weapon heat alone triggers the test. Hold Space for
several seconds, including after destroying the target, to reach a limit; release
to recover. Try thrust and turning while holding Space: steering gets first use
of the available energy, then thrust, then the gun. With insufficient energy,
steering/thrust operate at their native fractional strength. BACK still has no
reverse engine and spends nothing; simultaneous forward/back cancel.

Overheating uses the native strict upper threshold and recovery below90% of the
limit. It suspends generation, steering, thrust and firing. Native disabled drag
slows the ship gradually; healthy coasting retains velocity. A depleted ship with
no generation also drifts to a stop. This is the passive resource subset of
`NeedsEnergy`, not a full ship-disable implementation. R resets the shared budget
and movement together.

There is no heat hull damage, active cooling, repair, status-effect accounting,
ammunition or fuel cost in this slice. See [resource qualification](../resources/README.md)
and [propulsion qualification](../propulsion/README.md).

The fitted acceleration, turn rate and drag come from the qualified native
Sparrow trace, via `pilot_assets.py`; `PILOT.DAT` is a tiny prototype profile,
not a complete ship/outfit export. Movement remains60Hz while display targets30Hz.
The arrows/WASD use make/break input, including simultaneous held aliases;
reset/camera/Escape taps are retained until polled. See [input notes](INPUT.md).

## Watch

The build is staged in `.work/flight/run`. From the repository root:

```sh
python3 dos/flight/watch.py
```

Opens the reference Docker DOSBox through local X11/XWayland for30 seconds.
The latest build pans the camera horizontally and vertically, with slower star
parallax. Use `python3 dos/flight/watch.py --stationary` for a fixed view.
Escape exits early; Ctrl+F9 quits DOSBox. Windowed800×600, audio disabled.
It shares only the local X socket and, when available, the existing read-only
Xauthority file; it does not change `xhost`. Headless checks pass, and the user
confirmed both stationary and moving-camera runs successful and smooth on
2026-09-27; the exact launch route was not recorded.
A local-emulator alternative:

```sh
dosbox -conf .work/flight/demo.conf
```

Both watch commands above remain scripted; add `--pilot` for controls. Ignore
background traffic handling as a design statement. Frame cadence, sprite edges, soft transparency and the
overlapping flare are the useful things to inspect. Static preview:
`.work/flight/flight-6-ships-camera.png`. The preview comes from the DOS framebuffer;
VBE bank samples and all768 DAC components are also verified before exit.

## Moving-camera checkpoint at16MiB / fixed20,000 cycles

On the same360-frame route, cached static HUD strips and precomputed sprite span
addresses reduce average frame time from36.140ms to30.243ms (~33.1FPS uncapped).
The captured framebuffer is byte-identical before/after. Worst observed frame:
35.581ms, so this is **not a locked30FPS result**. With20 moving ships plus the
stationary Falcon, the camera route averages45.790ms (~21.8FPS).

HUD cost falls from3.543ms to0.709ms; fully visible sprites avoid repeated clip
arithmetic while edge-crossing sprites keep the checked clipped path. Backgrounds
and orbital objects are redrawn every frame; no stationary-camera cache hides
those costs. The changes add156,120 counted bytes (spans andHUD), bringing explicit
DOS residency to2,824,046 bytes (~2.69MiB), with the same exclusions below.

Detailed background/orbital/traffic/effect/HUD timings are in the camera reports.
The timed demo produced116frames and238motionticks over4.001seconds, with no
reported discarded simulation time. A native sanitizer test checks2,880ticks of
camera movement, its bounds/return to origin, unchanged world ship state, and63
complete rendered frames against the scalar rasterizer. All324 sprite clipping/
heading comparisons still pass. `flight-camera-qualification.json` records gates;
`flight-camera-before.json` preserves the pre-optimization baseline.

## Pilot integration checks

The native oracle now has14cases/12,600movement steps, including the exact
held-key route used by the sandbox. The native sanitizer integration test compares
all900 route ticks, control cancellation, reset and following/fixed cameras.
Headless DOS replay reaches the same final position, velocity and heading.
Four malformed pilot profiles are rejected without modifying the live state.

The actual application also passes X11 key injection through DOSBox: simultaneous
thrust/turn, release/coast, quick reset/camera taps and Escape. The separate input
probe checks original IRQ-vector restoration and reopening. These checks do not
replace human feedback on control feel. Captured view: `.work/flight/pilot-controls.png`.

The450-frame route averages18.57ms/frame, max38.32ms, but most of that journey
leaves the planet/traffic cluster: it is not an improvement claim against the
old camera route. The near-Earth keyboard test averages33.77ms, max38.18ms.
Player drawing averages1.49ms and two movement steps about0.07ms; the dynamic HUD
cost is also included. Sustained30FPS for a complete game remains unproven.
Explicit asset/world/frame allocations remain2,824,046B; player stack state and
DPMI keyboard-wrapper/runtime allocations are outside that count. No swap used.

Evidence: `flight-pilot-tests.json`, `flight-input-tests.json`,
`flight-controls.json`, `flight-6-ships-pilot-profile.json` and the real-time pilot
report under `dos/reports`. The original camera checks still pass, with additional
star-wrap checks24,000units away in either direction.

## Earlier stationary-camera measurements

| Scene/path | Draw average | Video upload | Total average | Reciprocal of average |
|---|---:|---:|---:|---:|
|6 moving ships + stationary Falcon, scalar pixel loop|154.54ms|6.29ms|160.86ms|6.2FPS|
|Same scene, span renderer|31.41ms|6.29ms|37.73ms|26.5FPS|
|20 moving ships + stationary Falcon, span renderer|53.48ms|6.29ms|59.82ms|16.7FPS|

These are uncapped scripted microbenchmarks, not native-game FPS. These earlier results predate the camera/fast-path work above. A sustained
30FPS game is still unproven; real simulation/audio/UI costs remain to be added.
The span version's final captured frame exactly matches the scalar baseline.
It skips zero-coverage regions, block-copies fully opaque spans, and uses the
same tables for partially transparent/additive spans.324 native sanitizer cases
compare all prepared headings and screen edges against the scalar reference.
Two malformed sprite files are rejected. Source and measurements live in
`dos/reports/flight-*.json`; active-world ownership evidence is recorded separately.

The earlier scene used2,667,926 bytes (~2.54MiB) of counted DOS heap allocations:
world store + owned Sol record,480,000-byte framebuffer, decoded sprites/spans,
and shared blend tables. This excludes runtime/code/stack/stdio/allocator metadata,
and480,000 bytes of video memory. Before/after free-page samples are not a peak.
Loading includes full pack validation and span preparation (~2.58s here).
No file I/O or world-record loading occurs inside the measured frame loop.

Benchmark mode advances exactly2 test-motion ticks per rendered frame. Demo
mode instead accumulates a60Hz clock, targets30Hz drawing, and catches up motion
when rendering is slower. A pause clamps pending simulation to250ms and reports
discarded time explicitly. The controllable ship and automated pilot replay use the qualified movement
kernel; background traffic still uses the earlier scripted test motion.

## Build and validation

Requires the cached images/toolchain, typed world pack, palette proof and native
motion oracle (`python3 dos/motion/check.py`) described in the parent README. All actual build/test commands use Docker:

```sh
python3 dos/flight/run.py
python3 dos/flight/run.py --ships 20 --frames 60
python3 dos/flight/run.py --camera --frames 360
python3 dos/flight/run.py --camera --headless-demo
python3 dos/flight/run.py --replay --frames 450
python3 dos/flight/run.py --replay --headless-demo
python3 dos/flight/test_power.py
python3 dos/flight/test_controls.py
python3 dos/flight/test_controls.py --resource-stress energy
python3 dos/flight/test_controls.py --resource-stress heat
python3 dos/flight/run.py --practice-test --resource-stress energy --frames 480
python3 dos/flight/run.py --practice-test --resource-stress heat --frames 900
# Optional reproduction of the slow correctness reference:
python3 dos/flight/run.py --reference
```

`run.py` compiles with DJGPP/i386/noMMX/SSE, runs headlessly with swap disabled,
checks guest success/tick counts/video readback, and converts the captured index
buffer to a PNG in Docker. It records input hashes and retains only the latest
artifact per tested scene. The last build determines what `watch.py` launches;
rerun the default build after experimenting with `--reference`.

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/work" -w /work endless-sky-dos-native-reference:local \
  python3 dos/flight/test_raster.py
```

## Asset/rendering contract

`assets.py` uses the provisional shared flight palette, unchanged. Source PNGs
are untouched; their original licenses/attribution remain in repository
`copyright`. This staging directory is a local experiment, not a release package.
Add all applicable notices before any redistribution.

ESSPRT1 files:8-byte magic `ESSPRT1\0`, uint16LE width,height,frames,mode.
For each frame:one index byte/pixel, followed by packed4-bit alpha for normal
mode0 (low nibble first). Mode1 has premultiplied additive color indices only.
Runtime normal coverage expands to one byte/pixel for simpler access; generated
spans group coverage classes. Fully transparent black remains distinct from
opaque black. Bounds constrain image size, frame count and allocations.

One shared16×256×256 alpha table costs1MiB; one additive256×256 table costs64KiB.
That is an intentional measured first pass, not a table per sprite/effect.
Offline quantization replaces per-pixel RGB arithmetic. Normal alpha uses exact
0/15 endpoints and14 precomputed intermediate levels; the additive source is
premultiplied once before palette mapping. Only one additive frame is exercised;
premultiplied-file variants, half-additive assets, animation interpolation,
faction recoloring and runtime rotation remain unqualified.

Next: shield regeneration, wider disabled-ship contracts and real hardpoints/beams. Bounded
unprotected shield/hull damage is now implemented; see ../damage/README.md.
Ordinary bolt traces and the first firing range are qualified in ../projectile/README.md;
complete combat and heavier scenes remain unqualified.

## Historical scripted-demo human check — 2026-09-27

User confirms the demo works, translational motion looks smooth and performance
feels stable. Abrupt turning was observed and explained: the script selects the
next of16 resident headings once per second (22.5 degrees), with no sprite reload.
Accepted as a rendering-demo checkpoint; the newer pilot uses64headings.
This feedback does not change the measured frame times or establish native
gameplay/physics equivalence.

Camera-only sanitizer gate: run `python3 dos/flight/test_camera.py` inside the
same native tools container used for `test_raster.py` above.
