# First DOS flight-scene prototype

A real800×600,256-color DOS executable now loads Sol from the compact world
pack and draws Earth, Luna, the Falcon, moving Sparrows/Star Barges and a small
additive effect. **This is a scripted rendering prototype, not playable Endless
Sky.** No native ship physics, AI, collision, combat, missions or menus yet.
Ship sprites retain their1× source size;16 baked headings are an experimental
rotation shortcut, not final turning fidelity. Orbital offsets follow the
source's clockwise angle convention at epoch0; Earth anchors the selected view.

## Watch

The build is staged in `.work/flight/run`. From the repository root:

```sh
python3 dos/flight/watch.py
```

Opens the reference Docker DOSBox through local X11/XWayland for30 seconds.
Escape exits early; Ctrl+F9 quits DOSBox. Windowed800×600, audio disabled.
It shares only the local X socket and, when available, the existing read-only
Xauthority file; it does not change `xhost`. Headless checks pass, and the user
confirmed the demo launches; the exact launch route was not recorded.
A local-emulator alternative:

```sh
dosbox -conf .work/flight/demo.conf
```

Both are scripted watch modes; there are no flight controls. Ignore ship handling
as a design statement. Frame cadence, sprite edges, soft transparency and the
overlapping flare are the useful things to inspect. Static preview:
`.work/flight/flight-6-ships.png`. The preview comes from the DOS framebuffer;
VBE bank samples and all768 DAC components are also verified before exit.

## Results at16MiB / fixed20,000 cycles

| Scene/path | Draw average | Video upload | Total average | Reciprocal of average |
|---|---:|---:|---:|---:|
|6 moving ships + stationary Falcon, scalar pixel loop|154.54ms|6.29ms|160.86ms|6.2FPS|
|Same scene, span renderer|31.41ms|6.29ms|37.73ms|26.5FPS|
|20 moving ships + stationary Falcon, span renderer|53.48ms|6.29ms|59.82ms|16.7FPS|

These are uncapped scripted microbenchmarks, not native-game FPS. The30FPS goal
is **not met yet**, and real simulation/audio/UI costs remain to be added.
The span version's final captured frame exactly matches the scalar baseline.
It skips zero-coverage regions, block-copies fully opaque spans, and uses the
same tables for partially transparent/additive spans.324 native sanitizer cases
compare all prepared headings and screen edges against the scalar reference.
Two malformed sprite files are rejected. Source and measurements live in
`dos/reports/flight-*.json`; active-world ownership evidence is recorded separately.

The scene uses2,667,926 bytes (~2.54MiB) of counted DOS heap allocations:
world store + owned Sol record,480,000-byte framebuffer, decoded sprites/spans,
and shared blend tables. This excludes runtime/code/stack/stdio/allocator metadata,
and480,000 bytes of video memory. Before/after free-page samples are not a peak.
Loading includes full pack validation and span preparation (~2.58s here).
No file I/O or world-record loading occurs inside the measured frame loop.

Benchmark mode advances exactly2 test-motion ticks per rendered frame. Demo
mode instead accumulates a60Hz clock, targets30Hz drawing, and catches up motion
when rendering is slower. A pause clamps pending simulation to250ms and reports
discarded time explicitly. It is a prototype clock, not a port of native physics.

## Build and validation

Requires the cached images/toolchain, typed world pack and palette proof described
in the parent README. All actual build/test commands use Docker:

```sh
python3 dos/flight/run.py
python3 dos/flight/run.py --ships 20 --frames 60
python3 dos/flight/run.py --headless-demo
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

Next: investigate remaining draw cost with representative camera motion before
choosing caching or further specialization; then native ship/motion behavior.

## Human check — 2026-09-27

User confirms the demo works, translational motion looks smooth and performance
feels stable. Abrupt turning was observed and explained: the script selects the
next of16 resident headings once per second (22.5 degrees), with no sprite reload.
Accepted as a rendering-demo checkpoint; smoother turning remains future work.
This feedback does not change the measured frame times or establish native
gameplay/physics equivalence.
