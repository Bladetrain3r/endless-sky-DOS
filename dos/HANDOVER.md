# Endless Sky DOS handover

Updated2026-09-27; typed snapshot/DOS validator pass, no playable DOS port yet.
User Wishlist.md delegates decisions and requests near-vanilla DOS,16MiB,
20kcycles,800x600 preferred,30FPS target; audio/setup and three benchmark presets.
Root read brief; two bounded Sol source audits reviewed; source remains unchanged.
Upstream061a9461a93898fb691504536589d1dcddc5d79b, CMake0.11.4; Git fork,
not Fossil. Wishlist.md is operator-authored; original text preserved byte-for-byte.

Completed: dos/README.md, FEASIBILITY.md, probe source/config/runner and JSON report.
Real DOS COM diagnostic in reused modern-arena-tools:0.1 (image ID in report),
network-disabled disposable Docker + Xvfb;1200frames,262BIOSticks (~11.992ms/fill),
800x600x8 banked VBE atnormal/pentium_slow/20kcycles/memsize16. All bank endpoints
verified. Not gameplay FPS, not full-game memory validation. Only latest .work
outputs retained/ignored. Separate Dockerfile.probe supplied but not built yet.

Operator additionally authorizes replacing logic for cheaper equivalent behavior;
no MMX/SSE dependency. Implementation may change radically; gameplay is contract.

DJGPP12.2/CWSDPMI7 initial runtime gate passed: GNU C++20,602453B executable,
concepts/span/map/string/vector/exceptions,4MiB touch; largest free14663680B before
allocation at16MB, swap disabled. dos/tools/run_runtime.py reproduces; JSON report.
Tools/cache327MiB reusable. EQUIVALENCE.md records math/RNG/gameplay test contract.

Native Release reference built/tested in Docker, source readonly. Data-only
parse95,088KiB peak RSS/0.17s, headless landing437,932KiB/5.51s; both pass, marker
verified. dos/reports/native-baseline.json holds raw logs/time, image/package refs.
No audio device; not a DOS heap estimate. Compiler artifacts85MiB. No build left
running. Native baseline is evidence only; compact data work now follows below.
Correct RNG option --rngseed (help --rng-seed is wrong in this checkout).
Root corrected delegate: headless tests already skip drawing; reuse main.cpp:621.
Window warns below1024x768, not hard reject;1000px menu requires800px UI reflow.
Preserve60Hzsimulation initially at30Hzrender;1MiB angle table is memory candidate.
Preserve original collision masks/logical dimensions independently of reduced art.
GPL3+ source and per-asset notices retained. MIDI conversion isn't codec switching;
new original scores allowed. No promises yet on16MiB/full parity/30FPS viability.
Local Git checkpoint for probes/docs; no push or external publication. Upstream
source/game data untouched. Resolve checkpoint with git log -1; central handover
records its ID after commit.

Storage A/B: dos/storage/README.md decision; reports/storage-baseline.json.
All9193source blocks/10,500,260B preserved and checked. DOS16MB/20k/no swap: pack
64KiBcache scan6.473s/random1.024k .917s/hot1.024k .865s, explicit225124B; SQL128
8.938/1.477/1.227s, SQLitepeak232192B (both exclude runtime/code/stdio). Pack181757B
exe vs SQL919505B; files10,573,820 vs12,328,960B. SQLite32cache thrashes hot set.
Choose pack for first read-only runtime content, cache optional for streaming.
SQLite runs on DOS via reviewed immutable VFS; NOT writable/save-qualified.
Rawblock comparison alone was not parsedworld/full-game RAM proof. Typed
snapshot milestone below supersedes its next-step pointer; game parity unproven.

Native ASan+UBSan storage gates passed including root Docker rerun; no leak claim.
Exact coverage/hash recorded in storage-baseline.json. Disposable sanitizer test
binaries removed after success; sources/fixtures remain reproducible.

Palette milestone: dos/palette/README.md + proof.py; nine real assets, six train/
three held out, Docker Pillow9.4.0. Provisional256 colors (16UI/32gray/208art),
six-bit DAC values, separate packed4-bit alpha roundtrip checked. Root visually
reviewed .work/palette/comparison.png; reference columns remain true-color.
reports/palette-baseline.json preserves hashes/errors; upstream assets untouched.
No DOS palette upload/render timing, special blending or swizzle qualification.
Typed slice now dos/world/README.md; pack+DOS validation, not active simulation.
1323records:694systems/619planets/10commodities,5518objects,1612links,4800prices.
648042Bdisk,37510B explicit heap(directory+scratch),~1.13s validation20k/16MB
no swap; reports/world-baseline.json. Exact nativeJSON field/reference roundtrip;
all native system/planet key+attribute rows and Sol/fixture orbit checks pass.
ASan/UBSan13corruption cases+5builder refusals; DOS badref/truncation reject.
Native --orbits --all crashes upstream; preserve failure, no full orbit-oracle claim.
Export tool stages introspection, consistently recompiles affected native objects;
original source/reference binary untouched. Fixture covers forwardrefs/add-remove/
overwrite/inaccessible links/multisystem planet/present zero trade price.
Next decoded active-system API, then flight scene additive effects/recolors.
Snapshot excludes mutable economy/events/ships/missions. No gameplay FPS claim.
