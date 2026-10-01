# Endless Sky DOS handover
Updated 2026-10-01. ESDOS-003 accepted on8828e304f; nightcap, development stopped.

## Current accepted build
- Run `python3 dos/flight/watch.py --campaign` from project root; Docker/X11,120s.
  First Pilot/First Light stock Sparrow,10000cr,empty15t hold, starts docked Earth.
  L depart; P destination; L approach/land; F5 dock save; Escape exit. Restart resumes
  last saved port. R cannot reset pilot; H/trainer gun inactive in this quiet mode.
- Autosave on landing/before departure; failed departure save keeps ship docked.
  Held F5 saves once, held L cannot bypass failed save; in-flight save refused.
- Oct1 human: Earth→Luna,F5,exit/relaunch at Luna; return Earth,exit/relaunch there.
  Manual+landing autosave accepted; no extra human refusal/reset/fault checks claimed.
  flight/CAMPAIGN.md documents controls, ownership, compatibility and limits.
- Two slots .work/flight/run/SAVE/PILOT0.SAV+PILOT1.SAV, last-good recovery; SEED.DAT
  host UUIDs only for initial creation. Build preserves SAVE. No native/user saves used.
  SAVEKEY.json+compiled header bind exact supported data/kernels; BUILD.json binds exe.
  Launcher checks binding and locks shared directory. DOS store single-writer only.
- Persistent IDs/credits/cargo/basis/date/location/fuel/crew; landed normalization.
  Native cargo profiles0–15t affect motion/heat capacity. No trade UI/jump/fuel burn,
  daily economy/date advance/missions/general fleet/profile policy/native interchange.
  Development format rejects incompatible builds; frozen epoch-zero Sol scene.

## Persistence evidence
- dos/persistence/{state,codec,store}.{c,h}; two bounded Sol delegates/root review.
  Codec is explicit little-endian, binary64,CRC32,bounded TLV; staged publication.
  Reject embedded NUL,invalid IDs/UTF8/counts/numbers; preserve future envelopes.
- Native ASan/UBSan/leak + DJGPP i386/noMMX/SSE codec/store/adapter gates pass:
  reports/persistence-tests.json;622 exhaustive native short writes,146 DOS samples
  (all header bytes/regular payload/end),all injected flush/commit/close/readback faults.
- DOS commit uses explicit INT21h/AH68h/carry checking; libc fsync tolerated some errors.
  Last-good slot/session remains intact on failure; late failure can leave recoverable
  newer candidate. No physical power-cut/FAT/controller durability claim.
- reports/persistence-application.json:actual IRQ/executable separate-process Earth→Luna
  autosave/restart,held F5/R,in-flight refusal,corrupt-newest recovery,future-file refusal,
  native after-purchase3t/8485cr identity across2launches. Screenshot inspected.
- Counted campaign resident2,914,462B; extra store heap bound262,404B separately reported.
  DPMI free pages3711→3079;16MiB/no swap. These are bounded scene data,not full-game peak.
- planning/PERSISTENCE-CONTRACT.md + NATIVE-SAVE-AUDIT.md; persistence/native.py links
  unchanged upstream,6purchase/reload/next-action/clamp/pending-sale/eligibility checks.
  native-limits.json exports stock capacities/crew/cargo-motion/heat; source/data hashes.
  DOS production next-trade differential gate belongs ESDOS-004,not implemented yet.

## Accepted trainers and retained evidence
- `--navigation`: stateless Earth/Luna, P/L,WAD cancellation,Space gun,R reset,H drain.
  Human quick landings at both accepted Sept30; ESDOS-001 closed. NAVIGATION.md.
- Navigation900frames2landings/2takeoffs,1682space+118dock ticks still pass;~26.576ms
  mixed route average is not whole-game FPS. Strict gates/held-L/service checks retained.
- `--arena`:2independent Barge trainers,2s grace; Tcycle/Nnearest,radar/cockpit.
  Human several rounds/HUD accepted;450frame900tick regression still passes.
- Prior `--pilot --pursuit`, `--pilot --incoming-fire`, quiet `--pilot` remain.
  R encounter reset,H25%shield drain,Esc/CtrlF9 exit; no persistent writes in trainers.
- F5 added IRQ decoder; native and actual DOS input/vector-restoration checks pass.
- Stock unassisted Sparrow motion; propulsion/gun shared power/heat in trainers.
  Generator1.9/tick > shield recharge .2; recharge consults prior energy.
  Human normal/energy/heat stress and pursuit budget behaviour accepted.
- Native motion/propulsion/projectile/collision qualified kernels remain in dos/;
  decimal parsing changed branch behaviour,so runtime profiles use exact binary64.
  Player shot spread uses separate trainer LCG; substitute blaster/explosion explicit.
- Pursuit shares Barge budget/lead helpers,not full vanilla AI. No ship collisions/audio.
- flight/ALIGNMENT.md:half-PNG world scale,64baked headings; native masks unchanged.
- World:694systems/648042B pack,619planets10commodities5518objects. Native all-orbits
  crash remains separate/unresolved; bounded Sol only. No general universe runtime.

## Queue, source and limits
- Beta=vanilla parity; flair afterward. VANILLA-PARITY.md83families,ROADMAP.md,TICKETS.md.
  LIFE02–04 partial only. ESDOS-002 done;003 done;004trade/005travel proposed.
- Next engineering: choose bounded two-port native commodity transaction fixture/UI,
  with clamping/zero-availability/retry/reload and loaded motion; define ticket before work.
- Upstream061a9461a93898fb691504536589d1dcddc5d79b,0.11.4,GPL3+/asset terms.
  source/data/images/Wishlist and user's screenshot untouched. Local commits,no push.
- /data/NuCode/Swarm_Workboard independent Fossil48tests; WB-002 integrity checker next.
  ESDOS Git tickets remain canonical. No Workboard deployment/model service.
- User also requested Space Denizen feature skeleton: docs/FEATURE-CHECKLIST.md there,
  local Fossil b745ea6b076a + wiki handover. Full survey after ES-DOS beta/chosen checkpoint;
  original game's priorities,not vanilla parity. No Space Denizen gameplay resumed.
