# Endless Sky DOS handover
Updated2026-09-27; native/DOS regular-flight equivalence passes; no playable port.

## Current checkpoint
- dos/flight/README.md: actual800x600x8 DOS VBE prototype, palette/DAC verification,
  Sol loaded through owned active-system API, Earth/Luna and source ship sprites.
- `.work/flight/flight-6-ships-camera.png` is latest captured DOS frame preview.
- Watch: `python3 dos/flight/watch.py` (Docker/local X11);30second scripted run,
  Escape exits; default now pans. --stationary restores oldview. Sept27 user confirms both stationary
  and moving-camera runs successful and smooth.
  Alternative local DOSBox: `dosbox -conf .work/flight/demo.conf`.
- Rebuild/bench: `python3 dos/flight/run.py`; --ships20 --frames60 for busier run;
  --headless-demo checks4seconds real-time; --reference reproduces scalar path.
- Latest staged executable uses optimized spans, not the scalar reference.
- Human accepts demo;22.5degree/1second heading snaps are known test behavior,
  not final turning. Exact human launch route not recorded; no new timings.
- 20k/16MiB/no swap camera360frames:6moving+Falcon30.243msavg (~33.1FPS),
  max35.581ms (NOT locked30);20moving45.790ms (~21.8FPS). Camera-before36.140ms.
- Cached static HUD + visible-sprite fast path/precomputed addresses; sameframe
  byte-identical. Background/planets still redrawn; source+evidence hashes saved.
- Scalar first pass160.858ms; span captured frame matches exactly SHA3013819d...
  Full hashes/configs/source/image refs in dos/reports/flight-*.json.
- DOS explicit allocations2,824,046B (~2.69MiB),+156120B for spans/staticHUD.
  Excludes code/runtime/stack/stdio/allocator metadata and480000B VRAM.
- Camera demo:116frames/238simticks over4001ms;0discardedms,60Hz test-motion clock
  separate from30Hz display target. Benchmark always2ticks/frame, uncapped.
- Scripted traffic/epoch0Sol view only; no native physics/AI/combat/collision/UI.
 16prebaked sprite headings, normal4-bit-alpha plus one additive effect;
 faction swizzles/half-additive/premultiplied assets/interpolation unqualified.
- Average clears30FPS in smallcamera scene; someframesmissbudget/fullgameunproven.
 Next integrate qualified motion into controllable scene; retain camera stress gates.

## Regular-flight kernel (separate from staged scene)
- dos/motion/README.md; check.py runs native Ship::Move oracle + portable C under
 native sanitizers and DOS.13cases/11700ticks; native exact, DOSpos5.46e-12,vel1.42e-14.
- All65536headings exact; unit-vector error1.11e-16; no1MiB lookup allocation.
- 64syntheticships600ticks ~884ms (~1.47ms/tick) at20k/16MiB/no swap.
 Motion only: noAI/resources/collision; healthy supplied ordinary flight contract.
- dos/reports/motion-equivalence.json records hashes/config/cost; .work/motion ~9MiB.

## Active world and validation
- dos/world/active.h,c:validated immutable store, name/ID lookup, owned decoded
 system arena; scratch never leaks into returned arrays/strings. Store256KiB cap,
 active arena64KiB cap. Native64bit max active6096B/store37558B; DOSsizesdiffer.
- Native ASan/UBSan root rerun:Sol/Sirius exact exported fields, all694systems,
 concurrentownership/100reloads/retentionafterstoreclose,5corruptionsreject.
- Raster ASan/UBSan324cases;camera2880ticks/63fullscene pixelcomparisons pass.
 `reports/flight-camera-qualification.json`; changedHUD/sprite samefinalpixels.
- Video backend:8banks,5samples/bank andall768DACcomponents verified. Earlier
 patternprobe passed; live flight also passes. No full VRAM hash claim.
- `dos/reports/flight-qualification.json` aggregates gates. No leak-check claim.
- Source modules below500LOC except active.c520 (readability over minification).
- Only~7MiB new ignored flight/active/video artifacts; no containers left running
 by these disposable probes. Existing other-project services not touched.

## Prior foundation (retained)
- User Wishlist.md unchanged; near-vanilla DOS,16MiB,20kcycles,800x600,30FPS target.
 User delegates implementation, permits equivalent rewrites, no MMX/SSE dependency.
- Upstream061a9461a93898fb691504536589d1dcddc5d79b,CMake0.11.4; Git fork not Fossil.
 Source/game data unchanged; local commits only, no push. Central HANDOVER has ID.
- `dos/world/README.md`:typed snapshot1323records(694systems/619planets/10commodities),
 5518objects/1612links/4800prices,648042Bpack. Validator37510Bheap/~1.13s at20k.
- Stable namespaced64bitIDs; native loader resolves forwardrefs/add-remove/
 overwrite/multisystem membership/zero-vs-absent trade. Source/export hashes saved.
- Everybinaryfield/reference nativeJSON roundtrip; original names/attributes and
 Sol/fixture orbit checks pass. Native --orbits --all crashes; bounded oracle only.
- Export tool stages friend access, consistently rebuilds47affected native TUs;
 original baseline binary untouched. Readonly snapshot excludes dynamic events.
- Storage A/B chose indexed pack for immutable content;SQLite DOS viable but
 writable saves unqualified. `dos/storage/README.md` and reports hold comparison.
- Palette256=16UI+32gray+208learned, six-bitDAC, separatecoverage;9assetproof user
 says barely sees difference. `.work/palette/comparison.png`; palette not frozen.
- RuntimeDJGPP12.2/CWSDPMI7GNU C++20 probe passes; flagsi386/noMMX/SSE, runtime
 library instructions not exhaustively audited. Cachedtoolchain327MiB reusable.
- Native baseline builds/headlesslandingpasses:95MiBparse/428MiBtestRSS, notDOSRAM.
- Preserve60Hz native simulation intent. Original collision masks/logicaldimensions
 must remain independent of reduced art. EQUIVALENCE.md owns future parity gates.
- GPL3+ source and perassetcopyright retained; generated assets local, not release.
 DOSBoxcycles are not calibrated PentiumMHz; audio/setup and fullgamebudget unproven.
