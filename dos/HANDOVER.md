# Endless Sky DOS handover
Updated2026-09-28; destructible firing range human accepted:7hits,explosion,R reset.

## Current checkpoint
- Flight: `python3 dos/flight/watch.py --pilot` (Docker/X11,120seconds).
 W/Up thrust,A/D/arrows turn,Space heldfire,Tab camera,R resetship/range,Esc exit.
 Stock Sparrow flight has no reverse; releasecoasts. CtrlF9 quitsDOSBox.
- Defaultpilot: Barge on8secondellipse, tangentfacing; --stationary-target retainsoldrange.
 Reduced30shield/26hull;7hitskill;48tickcosmeticsparkburst. R resetsall; backgroundcosmetic.
- Training1EnergyBlaster, synthetic centreline muzzle20units forward, unlimited
 resources. Original stockSparrow has2BeamLasers; no originaldata/outfit edits.
- No shipcollisions, AI, landing, missions, saves,regen or audio yet;gun unlimited.
 White7pixeltracers; no native projectileart/stretch/explosionlogic. --invulnerable-target keepsoldrange.
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
- ActualDOS600tickdamage range:50shots7hits0drops,25.932msavg30.762max at20kcycles.
 Heap2,897,942B counted; excludesruntime/stack+480000VRAM. Deathstopsmotion/collision.
 `pilot_draw` timing nowincludes practice target/bolts/label. Notfullcombatbudget.
- ActualDOSBox/XTest Space+flight/reset/camera/Esc and boundedshotcountafterrelease
 pass; no discardedsimtime. reports/flight-controls.json bindsstagedEXE.
- InputSpace=128heldlevel,nofiretaplatch. IRQprobe release/restore/reopen pass.
 Build/tests Docker; DJGPPi386/noMMX/SSE;16MiB/fixed20kcycles/CWSDPMI-s-.

## Damage and rendering qualification
- dos/damage/README.md:originalDamageProfile+Entity::TakeDamage onsynthetic
 unprotectedtarget;8cases18rows. NativeASanerror1.78e-15,DOS3.55e-15;deathflagsexact.
- Shieldoverflow scalesremaininghulldamage; native destroyedmeans hull<0,not<=0.
 Exactnativegetters exportedDAMAGE.DAT;DOStextparse passesgetter-basedzeroboundaries.
- NoShipdisable/events/protections/piercing/status/regen port. Blasterdisabledamage
 equalsnormalhull. Targethealthdeliberatelyreduced;notstockStarBarge.
- Integrationtests:7hitskill,noafterdeathhits,freeze movingtarget,resetmidexplosion,
 48effectages viewportguards/RNGunchanged. Rootinspected .work/flight/explosion-early.png.
- Current actualkeyboardtest includes7hitdestruction+healthchecks; profilehash inappreport.
- dos/motion/:14cases12600ticks;all65536directions pass;900tickpilot retained.
- Render:halfPNGworldscale/64headings;1188rastercases+3badfiles,63cameraframes;
 ALIGNMENT.md33samples,maskp95≤2px. Originalart/masks untouched.

## Collision / next
- dos/collision/: originalMask::Create3hulls123points;11772queries pass,
 strictcontains/hit;DOSmaxfractionerror4.44e-16. UsedbypracticeBarge now.
- Binary64transfer fixed212decimalboundarydifferences; defaultDOSFPUworks.
 Packedmask2060B; fixed3-maskstorage49392B. Originalimages/masksunchanged.
- Sept28 user: firing works, target hits look correct, no oddities moving/firing.
- Movingtests:postmovepose witness,480tickclosedroute,lead80/80 vsunled72/80,
 resetrepeatable(nativeASan+DOS). App2laps25.421msavg29.938max,heap2,897,898B.
- Sept28 user: static/moving fire work; leading predictable on steadyroute.
- Next: firing energy/heat and recovery;regen/disabledcontracts and beamsseparate.

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
