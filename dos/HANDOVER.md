# Endless Sky DOS handover
Updated2026-09-28; energy/heat firing trainer implemented; human playtest pending.

## Current checkpoint
- Flight: `python3 dos/flight/watch.py --pilot` (Docker/X11,120seconds).
 W/Up thrust,A/D/arrows turn,Space heldfire,Tab camera,R resetship/range,Esc exit.
 Stock Sparrow flight has no reverse; releasecoasts. CtrlF9 quitsDOSBox.
- Defaulttarget: Barge8secondellipse/tangentfacing,30shield/26hull,7hitskill,
 48tickproceduralsparks. --stationary-target fixesroute; --invulnerable-target retainsrange.
- One centreline EnergyBlaster20units forward, not stockSparrow's2BeamLasers.
 Gun costs energy/addsheat; HUDshowsenergy,heat,ready/reloading/lowenergy/overheated.
- Stock supply comfortably sustains onegun. `--resource-stress energy` deliberately
 sets40capacity/.25generation; `--resource-stress heat` sets450maxheat/0passiveheat.
 HoldSpace beyondtargetdeath toexercise limits; releasecools/recharges,R resetscold/full.
- Engines remain fullysupplied evenoverheated: explicit trainer exception.
 No shipcollisions,AI,landing,missions,saves,audio,shieldregen or heat hulldamage.
- User accepted flight/scale/64headingturning and static+movingfire/leading;
 Sept28 latesthumanacceptance:7hits,explosion,Rreset. Resourcebuild awaitsfeedback.
- Earliercorrectedflight .work/flight/accepted-44aced341; fullsize0636c12dc via
 watch.py --accepted. No publicpush; centralHANDOVER haslocalcommitID.

## Resource qualification
- dos/resources/README.md; native.py/oracle.cpp link unchangednativeobjects;
 actualShip::DoGeneration,CanFire,ExpendAmmo onfullhealthSparrow; test-onlyvisibility.
- Nativeprofile:4000capacity,1.9energy/tick,2.9heat/tick,.00077dissipation,
 18600maxheat,5.8energy/18heat perblastershot. Getter-derived RESOURCE.DAT.
- Clampenergy beforegeneration permits transientoverflow; dissipate beforechecking
 strict>max overheat/<.9max recovery; overheated suspendsgeneration. Fireaddsheat
 without immediatelylatchingoverheat. Startcold/full is trainerinitialization.
- 9cases927rows; nativeportableASan/UBSan exact,DOSmax4.55e-13; flags exact.
 4malformedprofilesrejectwithoutmutation. dos/reports/resource-equivalence.json.
- Root independentlyreproduced gates; composition also passes starvation/recovery,
 heatlock/recovery/reset,deniedshot RNG/reload/cost unchanged,pooldrop freeofcost.
- Source/data/hashchecked profile; bounded passive subset only, no movementcost,
 repair/status/fuel/solar/activecooling/hardpoint reload model or fullshipdisabling.
- Flight heap2,898,030B(+88); excludesruntime/stack/stdio/DPMIwrapper+480000VRAM.
 Heatstress1800ticks:53shots/1164blocked-readyticks,27.018msavg31.653maxat20k.
 Notfullcombatbudget. Rootviewed normal+overheated HUD captures under.work/flight.
- Actualkeyboardtest scripts have normal,energy/heat stress modes; reports
 flight-controls*.json bindstagedEXE. Reset/motion/fire/release/recovery verified.
- 900tickpilot replay stillmatchesnative. Build/tests Docker,DJGPPi386/noMMX/SSE,
 DOSBox16MiB/fixed20kcycles/CWSDPMI-s-. CyclesnotcalibratedMHz.

## Projectile and damage contracts
- dos/projectile/:nativeProjectile constructor/Move12cases600rows;
 588reachablethroughframe48;DOSmax7.11e-15,exactheading/dead/removalflags.
- Parentvelocityinherited; predecrementlifetime48 means47positionadvances.
 Existingboltsmove first; newshotsjoincollisionpasswithoutMove, likeEngine.
- Sweepcurrentposition→position+velocity againstpostmovetargetmasks; no relative
 targetvelocitysubtraction. Strictfraction<1,inside0,nearesthit,localorderties.
- Halfparentvelocity muzzlecorrection,draw+.5velocity. White7pixeltracers;
 instantremovalonhit differsfromnativeclippedfinalframe; no originalprojectileart.
- Reload12ticks/5Hz;3degree triangularspread/seededLCG,notnativeRNGsequence.
 16fixedslots/noshotalloc. R resetsstate/seed. dos/reports/projectile-equivalence.json.
- Integration:6sweepcases,50hits,expiry/poolsaturation84drops; postmove witness,
 480tickclosedroute;lead80/80 vsunled72/80; repeatedreset deterministic.
- dos/damage/:originalDamageProfile+Entity::TakeDamage,8cases18rows;
 DOSmax3.55e-15,exactdeathflag(hull<0,not<=0). Partialshields scalehulldamage.
- NoShipdisable/events/protection/piercing/status/regen. ReducedtargetnotstockBarge.
 Deadtargetstopsmotion/collision;resetmidexplosion and48effectagesguard/RNGtests pass.
- InputSpace128heldlevel/no taplatch; IRQrestore/reopen andsimultaneouskeysqualified.

## Other retained qualification
- dos/motion/:14cases12600ticks/all65536directions. Physics independentofart.
- RenderhalfPNGworldscale/64headings:1188rastercases+3badfiles;63cameraframes;
 ALIGNMENT.md33samples/maskp95≤2px. Originalart/masksunchanged.
- dos/collision/:3nativehulls123points11772queries;DOSfractionerror4.44e-16.
 Binary64transfer fixed212decimalboundarydifferences; defaultDOSFPUworks.
- dos/world/:all694systems;648042Bpack,619planets10commodities5518objects.
 Native --orbits --all crashes; boundedSolqualified. Indexedpackchosen,savesunqualified.
 DOSstore+scratch37510B;nativeAPIstore37558B,maxactive6096B.
- Sharedpalette256=16UI+32gray+208learned,6bitDAC. Userassetproofaccepted.
- OriginalWishlist.md intact; nearvanilla/16MiB/20k/800x600/30FPSaspirations.
 Upstream061a9461a93898fb691504536589d1dcddc5d79b,CMake0.11.4;GitnotFossil.
 GPL3+code/perassetcopyright; no bundled-library-wide ISA audit.

## Next
- Humanresourceplaytest, then propulsioncost/overheat-disabling contract or real
 hardpoints/beams. Shieldregen/disabledships separate; no newstep implicitlystarted.
