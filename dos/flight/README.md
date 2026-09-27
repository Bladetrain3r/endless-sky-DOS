# First DOS flight-scene prototype

The800×600,256-color DOS executable loads Sol from the compact world pack and
now offers a controllable stock Sparrow using native-matched movement. Earth,
Luna, the Falcon and scripted traffic provide reference points. This is a flight
sandbox: no collision, combat, landing, AI, missions, resource accounting or saves.
The player's engines are treated as fully supplied.16 baked sprite headings are
still an experimental rotation shortcut; simulation uses all65,536 headings.

User playtest2026-09-27: flight works and movement feels physically right;
operator notes limited familiarity with the original game.

Collision research found a scale correction still to make: this prototype draws
ships at full PNG size, while native `Body::Unit()` and collision masks use half
that size in world coordinates. Movement math remains qualified; shooting will
wait for that mapping to be corrected. See [collision notes](../collision/README.md).

## Fly

```sh
python3 dos/flight/watch.py --pilot
```

The reference Docker/DOSBox opens a two-minute flight session. Controls:

| Action | Keys |
|---|---|
| Thrust | W / Up |
| Turn | A/D / Left/Right |
| Coast | Release thrust |
| Toggle following/fixed camera | Tab |
| Reset ship and camera near Earth | R |
| Exit | Escape; Ctrl+F9 exits DOSBox |

Turn and thrust can be held together. The stock Sparrow has no reverse thrusters,
so S/Down does not brake. To slow down manually, turn around and thrust against
your motion; autopilot braking is not implemented. The camera starts following
at the ship's centre; Tab freezes its current position. R is useful after flying
out of sight. `--seconds N` selects1–120seconds. Local DOSBox alternative:
`dosbox -conf .work/flight/pilot.conf`.

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
python3 dos/flight/test_controls.py
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

Next: native ship/motion behavior against the reference, while retaining the
camera stress route and watching the remaining frame-time budget. Finer sprite
rotation and heavier scenes remain unqualified.

## Human check — 2026-09-27

User confirms the demo works, translational motion looks smooth and performance
feels stable. Abrupt turning was observed and explained: the script selects the
next of16 resident headings once per second (22.5 degrees), with no sprite reload.
Accepted as a rendering-demo checkpoint; smoother turning remains future work.
This feedback does not change the measured frame times or establish native
gameplay/physics equivalence.

Camera-only sanitizer gate: run `python3 dos/flight/test_camera.py` inside the
same native tools container used for `test_raster.py` above.
