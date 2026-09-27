# Endless Sky DOS handover
Updated2026-09-27; corrected native render scale +64headings staged; human check pending.

## Current checkpoint
- dos/flight/README.md:800x600x8 DOS VBE flight sandbox. Watch remains scripted;
  `python3 dos/flight/watch.py --pilot` flies stock Sparrow for120seconds.
- W/Up thrust, A/D or Left/Right turn; release to coast. No reverse engines:
  S/Down has no braking effect. Tab toggles follow/fixedcamera; R resets; Esc exits.
  --seconds1..120 supported; Ctrl+F9 quits DOSBox. No fullscreen/audio.
- Player uses native-matched60Hz movement; background6ships/Falcon remain scripted.
  No collision, AI, combat, landing, missions, saves or resource depletion yet.
 64baked sprite headings (5.625deg) now; physics uses65536directions.
- Prioraccepted fullsize build: watch.py --pilot --accepted; .work/flight/accepted-0636c12dc.
- User accepted stationary/mobile scripted camera Sept27; pilot human accepted Sept27: movement feels physically right.
- Source/game assets unchanged; local commits only/no push; central handover has ID.

## Flight validation and budget
- Pilot profile generated from native fitted Sparrow trace by pilot_assets.py;
  PILOT.DAT tiny validated profile, not fullship/outfit exporter. No guessed stats.
- 900tick native integration comparison passes everytick; headless DOS900tick
 replay same finalposition/velocity/heading. Reset/camera/command cancellation pass.
- Four invalid profiles reject without altering state. ASan/UBSan, no leak claim.
- Actual DOSBox/XTest keyboard run passes simultaneousthrust/turn, releases, quick
 taps, reset, camera and Escape; video samples/DAC pass; no discarded simtime.
- input.c locks IRQ1 state/code, restores vector; separate probe checks restore
 and reopen. Tap latches forR/Tab/Escape; see INPUT.md. Physicalkeyboard untested.
- Camera2880ticks/63scalar framecomparisons pass;1188heading/edge rastercases pass.
 Three malformed sprites reject, including65frames; ASan/UBSan no leak claim.
- Correctedscale camera360:21.189msavg/26.339max vs30.186ms prior fullsize.
 This is changed scene geometry, not a raster algorithm speedup/fullgame benchmark.
- NearEarth actualkeyboardrun23.784msavg/28.173max; playerdraw~0.67ms.
 Long450framepilotroute16.893msavg becomesempty; do not extrapolate to busyflight.
- Foursecond realtime replay117frames/238ticks,4006.779ms,0discard; native endstate.
- Counted assets/world/frame heap2,847,714B; excludes code/stack/stdio/allocator,
 DPMI keyboard wrapper/runtime allocations and480000BVRAM. DOS16MiB/20k/no swap.
- reports/flight-controls.json binds stagedEXE; camera/pilot-profile reports bindassets.
 run.py --replay --frames450 or --replay --headless-demo reproduces.
- test_controls.py drives actualapplication; test_input.py isolated IRQprobe.
 All builds/tests Docker; actual input test runs headless Xvfb/XTest.
- Sprite art halved offline, rotation atoriginalres thenLanczos resize; original
 sourceassets/masks unchanged. ALIGNMENT.md:33samples pass,maskp95≤2px; rootviewed.
## Regular-flight equivalence
- dos/motion/README.md/check.py: unchanged native Ship::Move oracle + portable C.
 14cases/12600ticks nativeexact; DOSposition5.46e-12,velocity1.42e-14 maxerror.
- Healthy/supplied ordinaryflight only; forward/coast/turn/back/STOP, reverseoutfit
 fixture andpilotroute. Fullresourceavailability andcrew checked in native oracle.
- All65536direction vectors pass(error1.11e-16); quantizedheadings exact.
 On-demand trig avoids native1MiB angle table; kernel allocatesnoheap.
- 64syntheticships600ticks ~884ms(~1.47ms/tick)at20k/16MiB/no swap; motiononly.
- reports/motion-equivalence.json has hashes/config; .work/motion ~9MiB.

## Collision foundation (not yet integrated)
- dos/collision/README.md/check.py: original Mask::Create outlines,3hulls/123points.
 11772native/DOSqueries pass strict hit/contains; maxfractionerror4.44e-16.
- Texttransfer caused212boundary disagreements; binary64 transfer fixes testedset.
 DefaultDOS andPC_53 bothpass; noFPUchange needed.8badmasksreject, no swap.
- Packedmasks2060B; fixedruntimebuffers49392B. reports/collision-equivalence.json.
- Native Drawable::Width/Height*.5 feedsDrawList; masks*.5. DOSart nowmatches.
 Sprite64frames cap; scripted16heading route maps toeveryfourth spriteframe.
- Trainer/demoscene soundtrack idea recorded in dos/WISHLIST.md; optional/unbuilt.

## Retained foundation and next work
- Next: qualify ordinary projectile lifetime/inheritedvelocity/swept hits
 before practice-fire integration; corrected sprite-size/turning human check pending.
- dos/world/active.h,c:owned decoded systemarena,validated immutable store. All694
 load,Sol/Siriusfieldchecks,retentionafterstoreclose,100reloads/corruptions pass.
- World648042Bpack:694systems/619planets/10commodities,5518objects/1612links/
 4800prices. Stable64bitIDs; native loader resolvesreference/modification semantics.
- Nativeexporter stagedfriendaccess rebuilds47affectedTUs consistently; no original
 mutation. --orbits --all crashes; bounded native oracles documented in world/.
- DOSstore+scratch37510B; native activeAPI store37558B,maxactive6096B (sizesdiffer).
- SQLiteDOS viable; indexedpackselected for immutable content. Savesunqualified.
- Palette256=16UI+32gray+208learned,6bitDAC; user9assetproof barelyseesdifference.
- Sprite spans preserve scalar pixels. Historical20moving fullsize~21.8FPS;
 correctedscale20ship budget unmeasured; completegame30FPS unproven.
- Upstream061a9461a93898fb691504536589d1dcddc5d79b,CMake0.11.4; Git fork,notFossil.
- RuntimeDJGPP12.2/CWSDPMI7, i386/noMMX/SSEflags; libraries notfullyISAaudited.
- Preserve originalcollisiongeometry independentofart. EQUIVALENCE.md owns contracts.
- Wishlist.md unchanged:nearvanilla,16MiB,20kcycles,800x600,30FPS target. DOSBoxcycles
 are notcalibratedhardwareMHz. GPL3+source/perassetcopyright; noassetreleaseyet.
