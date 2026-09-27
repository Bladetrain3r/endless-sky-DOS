# Endless Sky DOS handover
Updated2026-09-28; night checkpoint: shared propulsion/gun budget; playtest pending.

## Current build
- Launch `python3 dos/flight/watch.py --pilot` (Docker/X11,120seconds).
 W/Up thrust,A/D/arrows turn,Space fire,Tab camera,R resetall,Esc exit;CtrlF9 quitsDOSBox.
 StockSparrow has no reverse; healthy releasecoasts without drag.
- Moving Barge8secondellipse/tangentfacing,30shield/26hull,7hitskill,48ticksparks.
 --stationary-target fixesroute; --invulnerable-target retainsaimrange.
- Training1EnergyBlaster,centreline20unitsforward,notstock2BeamLasers.
 Propulsion andgun shareenergy/heat; HUD showsbudget andoverheat/lowgunenergy.
- Nativeorder: resourcegeneration→steering→thrust→weapon. Lowpower scales
 steering/thrust fractionally. Overheat blocksallthree andappliesdisableddrag.
 Coolbelow90%limit toresume; R restorescold/fullbudget andship/target.
- --resource-stress energy:40capacity/.25generation; heat:450max/0passiveheat.
 Stresspresetsdeliberate,notstock. HoldSpace+W+turn toexercisecompetition;
 releaseall torefill/cool. Normalprofile comfortablysupplies shortplaytests.
- Useracceptedflight/scale/64headingturning,static+movingfire/leading,7hitkill/reset;
 latest Sept28 userconfirmed gunoverheat/cooling andenergy-limitedfiring.
 Newlycoupledpropulsionawaits humanfeedback;userheadingtobed;nohumanresultyet.
- NoAI,shipcollisions,landing,missions,saves,audio,shieldregen or heathulldamage.
 Fullhull/crew disablelogic,afterburner/status/fuel/activecooling unported.
- Localcheckpoint/no push; centralHANDOVER hasID. Earliercorrectedflight
 .work/flight/accepted-44aced341; fullsize0636c12dc via watch.py --accepted.

## Shared power qualification
- dos/propulsion/README.md: actualnativeShip::Move thenoptionalExpendAmmo,
 stockSparrow/fullhealth/crew inSol. Nativeobjects unchanged,test-onlyvisibility.
- 17cases684rows/667steps: normal,tinybattery,turnsigns,cancel/back/coast,
 heatlock/recovery,NeedsEnergythresholds/zerocapacity,competinggun.
 NativeportableASan/UBSan andDOS havezeroobservedstateerror;angles/fire/flags exact.
 Fourmalformedprofilesrejectwithoutmutation;rootindependentlyreproduced.
- Nativecosts/tick:thrust1energy/1.8heat,turn.5energy/1.1heat atfullinput.
 No reverseengine means no BACKcost; cancellingforward/back alsospendsnothing.
- NeedsEnergy subset:requiresmovementenergy,positivecapacity,belowcapacity,
 energy<=.00390625 andnonpositivegeneration. Overheat/NeedsEnergy drift by1-drag.
- dos/resources/:4000capacity,1.9energy/2.9heatpertick,.00077dissipation,
 18600maxheat,5.8energy/18heatpershot. Clampbeforegeneration allows transientoverflow.
 9cases927rows nowzeroerror nativeANDDOS;4malformedprofiles. Strictheatthresholds.
- IMPORTANT:DOSdecimalreader altered even.5/.25; tinyremainder falselyenabled
 thrustdrag,causingmaterialtrajectoryerror. Fixexactbinarytransfer,NOTepsilon/snapping.
 Runtime RESOURCE.DAT:8byteESRES2\0\0+7LEbinary64;PROPULSE.DAT:ESPROP2\0+4LEbinary64.
 Rawgetterexportsstaytext;assets.stage converts+checksprovenance;tracesbinarytoo.
- dos/flight/test_power.py: generationonce/movementbeforegun,partialsteeringpriority,
 sharedoverheatdrift/recovery,healthycoast,reset/camera nativeASan+DOSpass.
 reports/flight-power-tests.json;propulsion-equivalence.json;resource-equivalence.json.
- 900tickappreplay stillsameacceptednativeposition/velocity/heading.
 reports/flight-controls*.json bindstagedEXE/profiles:stock+energy+heat actualkeyboard,
 thrust/turn/fire/reset/release/recovery,7hitkill,0discardedsimtime.
- Explicitheap remains2,898,030B;driveprofile32Bstack outsidecount. Excludes
 runtime/stack/stdio/DPMIwrapper and480000VRAM. No fullcombatFPSclaim.
 Build/testsDocker,DJGPPi386/noMMX/SSE,DOSBox16MiB/20k/CWSDPMI-s-;cyclesnotMHz.

## Earlier contracts and evidence
- dos/motion/:14cases12600ticks/all65536directions;fullysuppliedphysicsunchanged.
- dos/projectile/:12nativecases588reachableconstructor/Move rows,DOSmax7.11e-15.
 Inheritedvelocity;life48predecrement→47moves;existingmovefirst,newshotsquerybirthtick.
 Sweepspostmovetargetmasks,no relativevelocitysubtraction;strictfraction<1,inside0,
 nearesthit/localorderties. Nativehalfparentvelocitymuzzlecorrection;draw+.5velocity.
- 16boltsfixedpool,12tickreload/5Hz,3degree triangularseededLCG,nativeRNGdiffers.
 White7pixeltracers/immediatehitremoval arevisualdepartures. R clearsstate/seed.
 Regressioncovers50hits,84pooldrops,expiry/reset,postmovewitness,480tickroute,
 lead80/80 versusunled72/80; resourcegates stillpass withbinaryprofiles.
- dos/damage/:8nativecases18rows,DOSmax3.55e-15;partialshieldsoverflow,deathhull<0.
 ReducedtargetnotstockBarge;nodeadcollision/motion,resetmidexplosion/renderguards pass.
- RenderhalfPNGworldscale/64headings:1188rastercases+3badfiles,63cameraframes;
 ALIGNMENT.md33samples/maskp95≤2px. InputIRQrestore/reopen/heldSpace128 qualified.
- Collision:3nativehulls123points11772queries,DOSmaxfraction4.44e-16;
 earlierdecimalboundaryproblem alsofixedwithbinary64transfer. Originalart/masksunchanged.
- World:694systems/648042Bpack,619planets10commodities5518objects.
 Native --orbits --all crashes;boundedSolqualified;indexedpackchosen,savesunqualified.
- Palette256=16UI+32gray+208learned/6bitDAC,useraccepted. OriginalWishlistintact.
 Upstream061a9461a93898fb691504536589d1dcddc5d79b/CMake0.11.4;GitnotFossil.
 GPL3+code/perassetcopyright;no bundled-library-wideISAaudit.

## Next
- Human shared-power playtest whenrested; shieldregeneration is the proposed
 following slice, thenincomingfire. Neitherstarted; keepfurthernightwork bounded.
