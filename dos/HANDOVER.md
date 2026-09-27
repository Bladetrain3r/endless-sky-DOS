# Endless Sky DOS handover
Updated2026-09-27; controllable Sparrow staged; user reports flight works/feels physically right.

## Current checkpoint
- dos/flight/README.md:800x600x8 DOS VBE flight sandbox. Watch remains scripted;
  `python3 dos/flight/watch.py --pilot` flies stock Sparrow for120seconds.
- W/Up thrust, A/D or Left/Right turn; release to coast. No reverse engines:
  S/Down has no braking effect. Tab toggles follow/fixedcamera; R resets; Esc exits.
  --seconds1..120 supported; Ctrl+F9 quits DOSBox. No fullscreen/audio.
- Player uses native-matched60Hz movement; background6ships/Falcon remain scripted.
  No collision, AI, combat, landing, missions, saves or resource depletion yet.
 16baked sprite headings remain visibly coarse; physics uses65536directions.
- .work/flight/pilot-controls.png: root-inspected captured actual keyboard-run view.
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
- Original camera2880ticks/63scalar pixelcomparisons pass; new far-camera star-wrap
 cases pass. Raster kernels unchanged; prior324heading/clipping comparisons retained.
- Near-Earth keyboardrun33.77ms/frame avg,max38.18ms. Notlocked30FPS.
 Long450frame pilotroute18.57msavg,max38.32ms becomes emptier, not speedup claim.
- Playerdrawing~1.49ms/frame; two motionsteps~0.07ms. DynamicHUD~2ms included.
- Foursecond real-time replay116frames/239ticks,4029ms,0discardedms; native endstate.
- Counted assets/world/frame heap2,824,046B; excludes code/stack/stdio/allocator,
 DPMI keyboard wrapper/runtime allocations and480000BVRAM. DOS16MiB/20k/no swap.
- reports/flight-{pilot-tests,input-tests,controls}.json and pilot-profile reports
 hold evidence; run.py --replay --frames450 or --replay --headless-demo reproduces.
- test_controls.py drives actualapplication; test_input.py isolated IRQprobe.
 All builds/tests Docker; actual input test runs headless Xvfb/XTest.
- Watch default and --stationary retain oldscene; acceptedcamera framehash
 dc88c45658601c3a985f130d42d3b522b25441cc153ed505dfbd57498ba2df9d.

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
- Found scale bug: native Body::Unit*.5 andMask*.5; DOSart currentlyfullPNGsize.
 Correct renderer worldscale beforeweapons; acceptedflybuild untouched.
- Trainer/demoscene soundtrack idea recorded in dos/WISHLIST.md; optional/unbuilt.

## Retained foundation and next work
- Next: correct body/world rendering scale, then qualify ordinary projectile
 lifetime/inheritedvelocity/swept hits before practice-fire integration.
- dos/world/active.h,c:owned decoded systemarena,validated immutable store. All694
 load,Sol/Siriusfieldchecks,retentionafterstoreclose,100reloads/corruptions pass.
- World648042Bpack:694systems/619planets/10commodities,5518objects/1612links/
 4800prices. Stable64bitIDs; native loader resolvesreference/modification semantics.
- Nativeexporter stagedfriendaccess rebuilds47affectedTUs consistently; no original
 mutation. --orbits --all crashes; bounded native oracles documented in world/.
- DOSstore+scratch37510B; native activeAPI store37558B,maxactive6096B (sizesdiffer).
- SQLiteDOS viable; indexedpackselected for immutable content. Savesunqualified.
- Palette256=16UI+32gray+208learned,6bitDAC; user9assetproof barelyseesdifference.
- Sprite spans preserve scalar pixels. Acceptedcamera~33.1FPSavg/35.6msworst;
 20moving~21.8FPS; completegame30FPS unproven. No fixedcamera caching assumption.
- Upstream061a9461a93898fb691504536589d1dcddc5d79b,CMake0.11.4; Git fork,notFossil.
- RuntimeDJGPP12.2/CWSDPMI7, i386/noMMX/SSEflags; libraries notfullyISAaudited.
- Preserve originalcollisiongeometry independentofart. EQUIVALENCE.md owns contracts.
- Wishlist.md unchanged:nearvanilla,16MiB,20kcycles,800x600,30FPS target. DOSBoxcycles
 are notcalibratedhardwareMHz. GPL3+source/perassetcopyright; noassetreleaseyet.
