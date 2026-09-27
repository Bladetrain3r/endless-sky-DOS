# Feasibility and first decisions — 2026-09-27

Initial source audit, DOS graphics/runtime probes and native reference tests. Performance and memory
parity are open questions. This is an ambitious port, not a renderer-only patch.
Two Sol delegates supplied read-only simulation and renderer audits; root checked
key source references and corrected the headless-test and minimum-window claims.

## What the checkout establishes

- CMakeLists.txt:25 requires C++20. SDL2/3, OpenGL/GLEW, OpenAL, image decoders,
  music decoders and threading are dependencies, not DOS backends we can relink.
- source/Angle.cpp:30-56 allocates 65,536 double-coordinate Points: 1 MiB for this
  lookup alone. Shrinking/interpolating it is a candidate, not a free semantic
  change. source/Point.h already has a non-SSE path; do not require modern x86.
- source/UniverseObjects.cpp:40 loads data; source/Set.h preserves named object
  identity through map nodes and references. source/GameData.cpp:179 also copies
  default world registries for reset. source/DataFile.cpp:43 and DataNode.h's
  strings/tree containers imply parser peaks as well as steady state to measure.
- source/TaskQueue.cpp:29-55 uses worker threads; Engine.cpp:1123 queues simulation.
  DOS needs a synchronous/cooperative implementation preserving task ordering and
  completion semantics, not a pretend mutex/thread implementation.
- source/main.cpp:315,573 and the broader game rules assume 60 simulation steps
  per second. First choice: retain 60 Hz logic, render at 30 Hz. Measure whether
  two steps fit the draw interval. Don't halve the loop and halve the game speed.
- source/main.cpp:328,621 already supports headless integration execution;
  drawing is conditional. Seeded tests and command injection exist. Native
  reference testing can reuse these; DOS still needs platform/library replacement.
- source/Engine.cpp:1161 composes the draw pipeline; shader/DrawList.cpp and
  BatchDrawList.cpp provide useful rendering boundaries. Font rendering is also
  GL-backed (source/text/Font.cpp:125). Broadly preserve high-level call sites.
- source/GameWindow.cpp:171 warns below 1024x768, but does not hard reject smaller
  windows. source/Screen.cpp:94 assumes 1000 logical pixels for the main menu and
  floors zoom at 100%. 800x600 therefore requires layout work and hit-zone tests.
- source/image/ImageSet.cpp:202-250 derives collision masks from original frames.
  Engine.cpp:2438 uses these for combat. Downscale the display art without silently
  shrinking logical dimensions or changing combat geometry. Bake masks separately.
- source/audio/Music.cpp uses MP3/FLAC; Sound.cpp loads WAV for OpenAL. Sample
  conversion is straightforward in principle; converting recorded music to MIDI
  is transcription/arrangement, not a codec switch. Prefer new small original
  scores if existing arrangements cannot be reproduced well. Audio comes after
  the gameplay/rendering budgets, with cue identity and timing preserved.

File payload inventory (sum of regular file lengths, not runtime memory):

| Tree | Files | Bytes |
| --- | ---: | ---: |
| source | 486 | 3,617,748 |
| data | 204 | 10,500,260 |
| images | 6,922 | 346,207,696 |
| sounds | 247 | 46,187,732 |

The source data itself occupies about 10 MiB of text; parsed memory is not equal
to text size. Most art must stay on disk with a bounded cache. Compile data and
assets on the modern build host; avoid expensive image/music decoding and full
text parse trees in the DOS runtime. Preserve stable IDs, mutable world changes,
mission conditions and save references when moving away from pointer-rich maps.

## Measured graphics foothold

`reports/vbe-fill-baseline.json` records the exact source/config hashes, image ID,
packages and raw guest output. DOSBox normal core, pentium_slow, fixed20,000 cycles,
memsize16, SVGA S3, banked800x600x8: 1,200 frames in262 BIOS ticks, approximately
14.390 seconds, **11.992 ms/fill or83.39 fills/s**. Each frame writes480,000 bytes
with REP STOSW and eight bank selections. Post-timing endpoint checks passed for
every bank, including the partial last bank. Initial shorter120-frame trial was
26ticks; the longer run reduced clock quantization uncertainty.

This is NOT 83 FPS Endless Sky. It excludes source framebuffer reads/copies,
sprite rotation/blending, HUD, simulation, input, audio and DPMI overhead. It is
one observed fill path, not a theoretical lower bound for every renderer. A
30FPS frame has33.33ms; the measured fill alone uses about36% of that interval.
60FPS remains a stretch. No representative gameplay memory has been measured.
A COM diagnostic running with memsize16 does not prove the full game fits16MiB.

Working renderer candidate: 8-bit indexed software display with a bounded sprite
cache, clipped spans and preprocessed assets. Quantization, palette effects,
transparency and rotating ships need a visual prototype before committing to
that format. 800x600 is the first target. 1024x768 has64% more pixels and is a
layout escape hatch with a performance cost, not a faster fallback. RGB565 and
linear-framebuffer paths remain comparisons if the first prototype warrants them.
Two800x600 byte buffers cost960,000 bytes; video RAM and conventional/extended
memory are different budgets and must be reported separately.

## Native reference measured

Read-only upstream source built successfully in an Ubuntu24.04 Docker image;
Release, two compile jobs. `reports/native-baseline.json` retains image ID,
package versions, executable/script hashes, exact time output and logs.
Data-only `--parse-save --tq-threads 1` completed without parse errors:95,088KiB
peak RSS (~92.9MiB),0.17s wall. One headless test with seed1 called the upstream
“Landing in a system with multiple planets” and emitted a success marker after
return:437,932KiB peak RSS (~427.7MiB),5.51s wall. Integration fixtures are enabled;
this is not a clean stock-game-only heap count. ALSA had no audio device and PNG
metadata warnings occurred; audio was not qualified by this run. Build emitted a
nonfatal compile_commands symlink warning because the source mount is read-only.

Native RSS includes code, libraries, allocator/layout and loading costs. It is
not an exact DOS requirement, and the two peaks cannot simply be subtracted to
infer asset memory. Still, this is strong evidence to prioritize data/state and
asset residency work. NEXT: measure registry/string/object contributions and
prototype compiled data with paged narrative text and stable identities before
a broad renderer transplant. Preserve a small graphical slice in parallel only
where it resolves a distinct budget question. The passed reference is one test,
not certification of all upstream gameplay.

## Operator clarification

The operator explicitly authorizes replacing/rebuilding logic when it is cheaper
and produces equivalent behavior. Preserve behavior, not implementation shape.
No MMX/SSE requirement. Prefer measured compact representations, algorithms and
lookup/math changes; use reference traces and tolerances chosen for gameplay
consequences, not just pixel similarity. Combat outcomes, mission conditions and
save identity need explicit regression checks. Numerical drift is not silently
declared equivalent. This expands implementation freedom, not permission to
quietly reduce the game experience.

## Gates, in order

1. **Platform and reference.** Qualify a pinned DJGPP/DPMI C++ toolchain on DOSBox
   with actual library features used; capture executable/static footprint and
   available protected-mode memory with swapping disabled. Build the unchanged
   native game in Docker, run seeded headless tests and measure data-only and
   representative flight peaks. VBE and C++20/DPMI runtime probes passed; native data-only and one headless landing reference pass; broader coverage remains.
2. **Representative rendering.** Software menu with readable text and correct
   hit areas at800x600, then flight with original ship dimensions/masks, turning,
   background, HUD and radar. Use actual preprocessed game assets, not only dots.
   Measure rendering stages, sim ticks and allocation high-water marks separately.
3. **Playable parity slice.** New pilot, launch, move/fire, jump, land, trade,
   outfit, accept/complete a representative mission, save/load. Compare state
   outcomes to reference. Keep the full game's data vocabulary in scope; a small
   space shooter is an intermediate milestone, not completion of the request.
4. **Coverage and pressure.** Broaden mission/content/UI coverage; standard,
   busy-traffic and battle benchmark scenes with stable seeds and fixed durations.
   Record actor/projectile counts and sim-time progress beside frame rates. Never
   claim an optimization by slowing time or silently dropping gameplay actors.
5. **Audio/setup/package.** Sound Blaster samples, original MIDI/OPL score option,
   device setup, demos/headless result files, full attribution and clean install.
   Test within16MiB including runtime/data/caches/stack, no disk swapping. Separate
   package size from RAM, DOSBox host memory and emulated video RAM.

Crowd reductions are permitted by the brief only if the experience remains close;
mission-critical ships and battle outcomes need particular care. “Localized
 economy” may offer little gain if that work is already event-driven: profile
before designing an optimization for a workload we have not measured.

## Lessons to carry to Space Denizen

- Separate display cadence from simulation rules from the beginning.
- Visual simplification must not accidentally modify collision or targeting.
- System-defined fleet arrival periods and faction-strength checks
  (Engine.cpp:2015 onward) are worth studying for traffic tied to local conditions.
- Stable content identities and explicit budgets matter more than copying this
  engine's object graph. These are study notes, not new Denizen requirements.

## External implementation references

- [DJGPP project FAQ](https://www.delorie.com/djgpp/v2faq/faq2.html): protected-mode
  DOS toolchain family. Initial C++20/container/exception runtime probe passes; see README and report.
- [build-djgpp](https://github.com/andrewwutw/build-djgpp): cross-build route,
  including GCC12.2, now exercised by our runtime probe. C++20 syntax support alone doesn't establish usable DOS
  threading, filesystem or standard-library behavior.
- [DOSBox manual](https://www.dosbox.com/DOSBoxManual.html): fixed-cycle
  configuration. 20,000 cycles is our emulator test condition, not a claimed
  Pentium MHz equivalent; real486/Pentium qualification would be separate work.

Only compact reports/source/configuration are durable; `.work` is replaceable.
Keep original assets intact and package converted subsets with attribution.

Behavior-preserving replacements follow [EQUIVALENCE.md](EQUIVALENCE.md):
numerical tolerances, discrete gameplay boundaries and RNG replay are separate
checks. The help/parser RNG flag mismatch is recorded by the native runner: use
`--rngseed`, not the advertised `--rng-seed`, for this upstream checkout.

## Storage follow-up

The operator suggested an embedded database; the completed DOS comparison is in
[storage/README.md](storage/README.md). Both formats work; a cached indexed pack
is selected for the initial immutable content backend on this workload. Writable
state and crash recovery remain separate gates. Raw-record paging proves bounded
access to disk data, not bounded memory for fully parsed gameplay objects.
