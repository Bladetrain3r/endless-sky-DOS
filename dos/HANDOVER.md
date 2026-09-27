# Endless Sky DOS handover
Updated2026-09-28; moving firing range staged; human check pending.

## Current checkpoint
- Flight: `python3 dos/flight/watch.py --pilot` (Docker/X11,120seconds).
 W/Up thrust,A/D/arrows turn,Space heldfire,Tab camera,R resetship/range,Esc exit.
 Stock Sparrow flight has no reverse; releasecoasts. CtrlF9 quitsDOSBox.
- Defaultpilot: Barge on8secondellipse, tangentfacing; --stationary-target retainsoldrange.
 Indestructible; R resetsroute/seed/counters; HIT flash. Background traffic cosmetic.
- Training1EnergyBlaster, synthetic centreline muzzle20units forward, unlimited
 resources. Original stockSparrow has2BeamLasers; no originaldata/outfit edits.
- No damage/shields, shipcollisions, AI, landing, missions, saves or audio yet.
 White7pixeltracers; native projectile art/stretch/impacteffects notported.
- User Sept28 accepted corrected smaller worldscale and much smoother64headingturns.
 Prior Sept27 accepted controllableflight feels physicallyright; limitedoriginal familiarity.
- Preserved correctedflight .work/flight/accepted-44aced341; earlierfullsize
 .work/flight/accepted-0636c12dc via watch.py --accepted. No backgroundcontainers.
- Localcommit only/no push; centralHANDOVER has ID. Originalsource/assets untouched.

## Projectile qualification
- dos/projectile/README.md, native.py +oracle.cpp use unchanged nativeobjects,
 publicProjectile constructor/Move +loadedEnergyBlaster;12cases600rows.
- PortableC compares588reachable rows (creation..move48); nativeexact,
 DOSmaxstateerror7.11e-15 against1e-10limit; heading/dead/removalexact.
- Native: parentvelocity inherited; predecrementlifetime48,47positionadvances.
 Existingboltsmove thenexpiredprune; newshotsjoincollisionpass withoutMove.
- Queries currentposition→position+velocity againstpostmove target snapshots;
 originalMask geometry, no targetvelocitysubtraction. No fullEngine/factionclaim.
- Centrelineorigin uses native halfparentvelocitycorrection; rendering+.5velocity.
 Immediatehitremoval vsnativefinalclippedrender is explicitvisualdeparture.
- Reload12ticks/5Hz,3degree triangularspread withseededLCG, notnativeRNGsequence.
 R clearsbolts/counters/cooldown/seed. 16fixedslots,no per-shotallocation.
- NativeASan/UBSan +DOS tests: nearest/tunnelling/tie/inside/miss/endpoint/expiry,
 50hits,release/reset,expirywithouttargets,poolsaturation drops84withoutoverwrite.
- reports/projectile-equivalence.json bindsoracle/kernel/profile/masks/config.
 `python3 dos/projectile/check.py --reuse-oracle` useshashcheckedfixtures.
- ActualDOS600tickrange:50shots50hits0drops,25.274msavg29.985max at20kcycles.
 Heap2,897,866B counted incl50,152Btrainingstate; excludesruntime/stack+480000VRAM.
 `pilot_draw` timing nowincludes practice target/bolts/label. Notfullcombatbudget.
- ActualDOSBox/XTest Space+flight/reset/camera/Esc and boundedshotcountafterrelease
 pass; no discardedsimtime. reports/flight-controls.json bindsstagedEXE.
- InputSpace=128heldlevel,nofiretaplatch. IRQprobe release/restore/reopen pass.
 Build/tests Docker; DJGPPi386/noMMX/SSE;16MiB/fixed20kcycles/CWSDPMI-s-.

## Flight / rendering retained evidence
- 900tickpilot replay afterintegration stillmatches originalnativeendstate;
 current17.777msavg becomesempty, notbusyflight performanceclaim.
- dos/motion/:14cases12600ticks; full65536directionvectors pass.
 Healthy/suppliedregularflight only, noafterburner/status/externalforces.
- InputIRQ1 lockedcode/data,originalvectorrestore; INPUT.md coverslatchlimits.
- NativeDrawable::Width/Height*.5 feedsDrawList; art halvedoffline independently
 oforiginalmasks.64spriteheadings,physics65536. SourceRGBA untouched.
- Raster1188cases+3badfiles, camera2880ticks/63frames, ASan/UBSan passSept27.
 ALIGNMENT.md:33samples,maskp95≤2px; rootinspected. Nearestangleerror≤2.8125deg.
- Historicalcorrectedcamera21.189msavg26.339max; completegame30FPS unproven.
- .work/flight/run currentEXE/profile/assets; run.py --practice-test --frames300
 reproducesrange; run.py --replay --frames450 checks flight endstate.

## Collision foundation / next
- dos/collision/: originalMask::Create3hulls123points;11772queries pass,
 strictcontains/hit;DOSmaxfractionerror4.44e-16. UsedbypracticeBarge now.
- Binary64transfer fixed212decimalboundarydifferences; defaultDOSFPUworks.
 Packedmask2060B; fixed3-maskstorage49392B. Originalimages/masksunchanged.
- Sept28 user: firing works, target hits look correct, no oddities moving/firing.
- Movingtests:postmovepose witness,480tickclosedroute,lead80/80 vsunled72/80,
 resetrepeatable(nativeASan+DOS). App2laps25.421msavg29.938max,heap2,897,898B.
- Next: humanmovingrange feedback +native shield/damage/resourcecontracts; beamsseparate.
- Optionaltrainer+demoscenemusic in dos/WISHLIST.md; noaudioimplementation.

## Retained content / boundaries
- dos/world/active.h,c:ownedvalidatedsystemarena;all694systems qualificationpass.
 648042Bpack:694systems619planets10commodities5518objects1612links4800prices.
- Nativeexporterstagedfriend rebuild47TUs; sourceunchanged. Native --orbits --all
 crashes; boundedSolorbitqualified. SQLiteDOSviable;indexedpackchosen,savesunqualified.
- DOSstore+scratch37510B;nativeactiveAPIstore37558B,maxactive6096B.
- Sharedpalette256=16UI+32gray+208learned;6bitDAC. Userassetproofaccepted.
- OriginalWishlist.md intact;nearvanilla/16MiB/20k/800x600/30FPSaspirations.
 CyclesnotcalibratedMHz; GPL3+code/perassetcopyright; nolibrary-wideISAaudit.
- Upstream061a9461a93898fb691504536589d1dcddc5d79b,CMake0.11.4;GitforknotFossil.
- Maintainmodularsoft500LOC; preservephysicsindependentofart; EQUIVALENCE.md.
