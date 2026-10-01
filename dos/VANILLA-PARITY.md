# Vanilla feature parity → ESDOS beta

Survey checkpoint: 2026-09-30. User milestone: **vanilla feature parity is beta;
our additional flair follows beta toward ESDOS 1.0.** This file is the coverage
ledger. [ROADMAP.md](ROADMAP.md) orders the work; [TICKETS.md](TICKETS.md) records
bounded assignments. A coverage item is not automatically a ready coding ticket.

## Target and evidence

Working baseline: upstream `061a9461a93898fb691504536589d1dcddc5d79b`, reporting
version 0.11.4, already present in this checkout. Freeze source, data and test
fixtures for beta qualification. A future upstream update gets a separate delta
survey; this is not a promise to chase every later release.

Primary evidence is the local source and data, with detailed audited references:

- [Flight, combat, AI, fleets and world](planning/VANILLA-FLIGHT-SURVEY.md).
- [Campaign, economy, ports, persistence and content](planning/VANILLA-CAMPAIGN-SURVEY.md).
- [Reproducible content inventory](reports/vanilla-feature-inventory.json), generated
  by `python3 dos/tools/feature_inventory.py`. 204 data files, 9,193 raw declaration
  headers: 2,344 mission, 439 event, 938 outfit, 917 ship, 694 system, 619 planet,
  128 government, 223 fleet, 34 minable, 30 hazard, 18 wormhole and 5 start headers.
  These include variants, overrides and deprecated material; they are **not**
  unique resolved objects or counts of playable stories. Inline conversations are
  not included in the 54 named conversation headers.
- Upstream [player manual](https://github.com/endless-sky/endless-sky/wiki/PlayersManual)
  and [project overview](https://endless-sky.github.io/) were cross-checks for the
  player-facing categories; local pinned code governs differences with live docs.

This is a broad source survey, not execution of every story branch or proof that
no undocumented behavior remains. Native rules with no stock-content example
are identified during implementation rather than invented into game features.
Maintain a schema/branch coverage matrix as the engine grows. Do not call parser
success, raw object counts, or one completed campaign universal parity.

## Reading the checklist

Every row is unchecked because **no complete feature family is yet qualified as
beta-ready**. `P` means a bounded part exists with evidence, `N` means not integrated
into the playable DOS game, and `D` means the DOS compatibility/adaptation boundary
needs a decision. N does not discount useful existing export/oracle work.

To check an item, link a source revision, native contract/fixture, DOS comparison,
integration scenario and human evidence where relevant. State deliberate
substitutions and limits. Helper parity, integrated behavior and human acceptance
are separate evidence. A checkbox must not silently waive a subfeature in its row.
No percentage completion or delivery date is inferred from row counts.

## A. Player lifecycle and persistent state

Source: `StartConditions`, `PlayerInfo`, `PilotProfile`, `SavedGame`, `LoadPanel`,
`Gamerules`, `GamerulesPanel`; campaign audit §§1,12.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | LIFE-01 | N | All valid vanilla starts, unlock/display conditions, naming, initial ship/date/credits/debt/conditions and introduction; compare initialized native state. |
| [ ] | LIFE-02 | P | Player-owned persistent ship/fleet/cargo/accounts/location/knowledge model with stable identities; avoid trainer reset state masquerading as a pilot. |
| [ ] | LIFE-03 | P | Landed save/autosave policy, pilot selection, named snapshots, restore/delete; match eligibility and next action after reload. |
| [ ] | LIFE-04 | P | Save all mission instances, UUID links, event queue, mutable universe, economy, logs, storage, ships and conditions; interrupted save preserves last good state. |
| [ ] | LIFE-05 | N | Death, pilot locking and vanilla gamerules: permadeath/save restrictions, fleet limits, presets and locking; default behavior plus configurable variants. |
| [ ] | LIFE-06 | D | Native save import/export compatibility and supported version range. DOS save/load is mandatory; interchange format is a separate proposed capability. |

Sept30 evidence: [persistent pilot](flight/CAMPAIGN.md) covers one stock ship,
credits/cargo/date/location, port autosaves and failure recovery with native/DOS
and actual restart tests. Human Luna/Earth restart acceptance recorded2026-10-01. Full profile/fleet/mission/
event/universe saves, pilot UI and next trade remain open; no family is complete.

## B. Flight, navigation and exploration

Source: flight audit §§1–3,12–15; `Command`, `ShipJumpNavigation`, `RoutePlan`,
`System`, `Wormhole`, `PlayerInfo`.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | NAV-01 | P | General ship movement, turn/thrust/reverse/stop/afterburner, mass/loadout and crew effects; current stock Sparrow kernel is only a subset. |
| [ ] | NAV-02 | P | Full shared resource generation/use, heat/cooling, fuel, solar/ramscoop, regeneration/repair delays and disabled recovery; compare ordered thresholds. |
| [ ] | NAV-03 | P | Player commands, targeting/nearest/asteroid targets, secondary selection, steering/aim assists and cancellation rules; all commands remain reachable. |
| [ ] | NAV-04 | P | General landing/takeoff, body radii, permissions/restrictions/bribes, escort behavior, interruption and service boundary; Earth/Luna slice exists, human check pending. |
| [ ] | NAV-05 | N | Hyperdrive, scram and jump-drive readiness, range/mass/fuel costs, departure/arrival, silent/custom capabilities and fuel exhaustion. |
| [ ] | NAV-06 | N | Wormhole accessibility, destinations and map reveal; travel must preserve ship/fleet/mission state. |
| [ ] | NAV-07 | N | Multi-leg routes, drive-aware pathfinding/fuel/day costs, queued travel, refueling/stranded cases and coordinated fleet jump. |
| [ ] | NAV-08 | N | Seen/named/visited/hidden/shrouded/inaccessible knowledge, map purchase/reveal, travel-plan persistence; unknown information must remain hidden. |
| [ ] | NAV-09 | P | Complete active-system loading/unloading, nested dated orbits and object placement; static world pack and bounded Sol exist, arbitrary-system orbit qualification remains open. |
| [ ] | NAV-10 | N | Ambient fleets/persons, variant/cargo/entry distribution, reinforcement/raid behavior, government identity and intentional persistence/forgetting. |
| [ ] | NAV-11 | N | Hazards/weather, solar input, environmental damage/status and AI boundaries; compare seeded duration, reach and effects. |

## C. Combat, interaction and harvesting

Source: flight audit §§4–7,10–11,16; `Armament`, `Weapon`, `Projectile`,
`DamageProfile`, `Entity`, `Engine`, `BoardingPanel`, `CaptureOdds`, `Politics`.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | FIGHT-01 | P | General entity lifetime and native tick order across movement, firing, hits, scanning, collection and removal; trainer fixed pools/order are not the full engine. |
| [ ] | FIGHT-02 | N | Real mounts/muzzles, gun/turret arcs, tracking/aiming, primary/secondary groups, streamed/cluster/burst fire, ammo and reload. |
| [ ] | FIGHT-03 | P | Ordinary bolts plus beams, homing/sensors/jamming/confusion, acceleration, drag, lifetime and inaccuracy distributions. |
| [ ] | FIGHT-04 | N | Blast/proximity, submunitions and spawn causes, penetration/phasing, safe/selective collision, anti-missile and tractor weapons. |
| [ ] | FIGHT-05 | P | Original mask geometry, swept hits, eligible targets, asteroid occlusion, friendly-fire and area rules; current mask/ordinary-bolt tests are bounded evidence. |
| [ ] | FIGHT-06 | P | Shield/hull/disabled damage plus heat/energy/fuel, relative damage, force, piercing/permeability/protection and prospecting. |
| [ ] | FIGHT-07 | N | Discharge, corrosion, ionization, burning, leakage, slowness, scrambling and disruption; application, decay/resistance costs and action consequences. |
| [ ] | FIGHT-08 | P | Disable/recovery, destruction staging, explosion effects, cargo/outfit drops and carried-ship consequences; trainer freeze/hide is partial. |
| [ ] | FIGHT-09 | N | Cloak transitions, fuel cost, detection, phasing/protection and action-specific permissions; loss/reacquisition and resource failure. |
| [ ] | FIGHT-10 | P | Full AI target selection, personalities, tactics, range keeping, flee/appease, ramming, surveillance, rescue, mining and autonomous travel; Barge pursuit is a helper subset. |
| [ ] | FIGHT-11 | N | Boarding approach, rescue/repair, fuel/energy transfer, plunder/cargo limits, stolen ammo, self-destruct and boarding mission hook. |
| [ ] | FIGHT-12 | N | Crew capture combat, attack/defend odds/casualties, successful/failed capture and fleet ownership/capacity consequences. |
| [ ] | FIGHT-13 | N | Cargo/outfit scan progress, range/opacity, shared scan effort, inscrutable/silent scans, provocation and results. |
| [ ] | FIGHT-14 | N | Hail ship/planet, translation/communication restrictions, help/refuel/bribe/tribute paths and messages. |
| [ ] | FIGHT-15 | N | Offenses, reputation/hostility/fines, enforcement, contraband/security and landing/service consequences across saves. |
| [ ] | FIGHT-16 | N | Decorative asteroids vs minables, eccentric motion, damage/prospecting and seeded payload drops; target/mine/harvest loop. |
| [ ] | FIGHT-17 | N | Flotsam lifespan/drag/ownership, partial collection, cargo capacity and collection preferences; competing tractor beams. |

## D. Fleet and carriers

Source: flight audit §§8–9,17; campaign audit §11; `orders/OrderSet`,
`FormationPositioner`, `ShipManager`, `PlayerInfo`, `Ship`.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | FLEET-01 | N | Own/select/group/reorder/rename ships, switch flagship, park/unpark, local/remote/disabled state and persistent UUID relationships. |
| [ ] | FLEET-02 | N | Fight/finish-off, hold fire/position, gather/move/keep-station, mine/harvest/scan, ammo policy; compatible orders and invalid-target cleanup. |
| [ ] | FLEET-03 | N | Formations, parent/escort following, jump waiting, independent remote routes and reunion; heterogeneous fuel/capability cases. |
| [ ] | FLEET-04 | N | Typed fighter/drone bays, deploy/recover, launch positions, carried resource generation/repair/resupply, cargo transfer and carrier death. |
| [ ] | FLEET-05 | N | Fleet capacity/crew/accounting policies, mission NPCs/gift/take/disown and persistent special actors. |

## E. Ports, commerce and ship progression

Source: campaign audit §§6–11; `PlanetPanel`, `Port`, `TradingPanel`, `Account`,
`CargoHold`, `FleetCargo`, `OutfitterPanel`, `ShipyardPanel`, `HiringPanel`.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | PORT-01 | P | Description/landscape and correct menu/service availability at every valid port; two descriptions and simple dock menu exist. |
| [ ] | PORT-02 | P | Authorized fuel/hull/shield/energy/crew service, restricted/uninhabited ports, clearance/infiltration and landed/takeoff mission transitions. |
| [ ] | PORT-03 | N | Credits/commodity purchase and sale, quantity controls, affordability/capacity/stock constraints, basis/profit and repeated transactions. |
| [ ] | PORT-04 | N | Dynamic commodity supply/prices, local production/use, inter-system exchange and pending purchases; seeded multi-day trace plus reload. |
| [ ] | PORT-05 | N | Commodity/outfit/minable/mission cargo and passenger/bunk identities, landed pooling, fleet redistribution and remote/parked ships. |
| [ ] | PORT-06 | N | Planetary outfit storage, transfers and sale of plunder/minables; fractional/massless items and full-capacity cases. |
| [ ] | PORT-07 | N | Bank loans/debt/fines/interest/payoff/prequalification/credit history, salaries/arrears/income/maintenance/tribute and date advancement. |
| [ ] | PORT-08 | N | Hire/fire crew, requirements/bunks, casualties and under-crewing effects, wages and takeoff checks. |
| [ ] | PORT-09 | N | Outfitter buy/sell/install/remove/store, capacities/categories/licenses/depreciation, local stock/refill and multiship operations. |
| [ ] | PORT-10 | N | Apply every stock outfit's gameplay attributes to resulting ship behavior, including special systems; validate invalid/flightless loadouts. |
| [ ] | PORT-11 | N | Shipyard model/variant availability, buy/sell chassis vs equipped ship, licenses/depreciation/naming and fleet/account updates. |
| [ ] | PORT-12 | N | Spaceport descriptions/news/conversations and job-board offers; changed conditions/services must refresh correctly. |

## F. Missions, stories and mutable universe

Source: campaign audit §§1–5,13; `Mission`, `MissionAction`, `GameAction`,
`Conversation`, `ConditionSet`, `ConditionAssignments`, `ConditionsStore`,
`GameEvent`, `UniverseObjects`, `LocationFilter`, `NPC`, `NPCAction`.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | STORY-01 | N | Native condition/expression/assignment language, integer boundaries, scopes and live derived providers; corpus/invalid-input comparisons. |
| [ ] | STORY-02 | N | Mission template instantiation, randomness/substitution/location filters, priorities/repeats, offered vs active identity and UUIDs. |
| [ ] | STORY-03 | N | All offer contexts: spaceport, landing, job, assisting, boarding, shipyard, outfitter, job board, entering and transition. |
| [ ] | STORY-04 | N | Accept/decline/defer/complete/fail/abort/visit/stopover/waypoint/daily/disabled triggers plus enter/land actions; exactly-once transitions. |
| [ ] | STORY-05 | N | Cargo/passengers, deadlines/timers, waypoints/stopovers, markers/visibility, clearance/infiltration, contraband and impossible mission handling. |
| [ ] | STORY-06 | N | Mission NPC fleets/personality/objectives, ship-event responses, escort/capture/destruction and persistent actor state. |
| [ ] | STORY-07 | N | Conversations: paragraphs/scenes/choices/conditions/gotos/actions/name entry/outcomes, readable navigation and long text. |
| [ ] | STORY-08 | N | Actions: money/fines/debt, ships/outfits, conditions, logs/messages/music, map changes, mission failure and scheduled events. |
| [ ] | STORY-09 | N | Persistent event queue and mutable planet/system/government/fleet/shop/news/link/wormhole/substitution state, raw/deferred changes and reference repair. |
| [ ] | STORY-10 | N | Daily calendar/order of costs/economy/events/missions; same-day conflicts and repeated load must not duplicate effects. |
| [ ] | STORY-11 | N | Every pinned vanilla content family loads with classified diagnostics; coverage registry maps declarations to resolved definitions and used schema features. |
| [ ] | STORY-12 | N | Campaign regression suite: authored representative routes for every content family plus branching/failure variants and a complete major campaign path; link semantic branch coverage and outstanding exceptions. |

Content families to account for include `human`, `hai`, `wanderer`, `remnant`,
`korath`, `coalition`, `quarg`, `pug`, `drak`, `sheragi`, `successors`, `bunrodea`,
`gegno`, `avgi`, `kahet`, `incipias`, `rulei`, `iije`
and `vyrmeid`, plus shared root data and `_deprecated` compatibility definitions.
This list describes directories, not a promise that each contains a finished
campaign. Keep plot-specific route details in separate fixtures to avoid spoilers.

## G. Information, interface and accessibility

These are functional UI requirements; DOS layout may differ. Source anchors are
`Command`, `MainPanel`, `MapPanel`, `MapDetailPanel`, `MapSalesPanel`,
`MapOutfitterPanel`, `MapShipyardPanel`, `PlayerInfoPanel`, `ShipInfoPanel`,
`MissionPanel`, `LogbookPanel`, `MessageLogPanel`, `PreferencesPanel`,
`ConversationPanel`, `Interface` and `text/`.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | UI-01 | P | Flight HUD/resources, target information, radar/minimap, escort/ammo status, warnings/offscreen indicators and paused/menu state; current trainer HUD is partial. |
| [ ] | UI-02 | N | Galaxy/system map, pan/zoom/search, route edits, mission/escort markers, political/commodity information and known-port facilities; preserve knowledge limits. |
| [ ] | UI-03 | N | Ship/outfit sales maps, comparison and search; distinguish known availability from hidden/unvisited content. |
| [ ] | UI-04 | N | Pilot/account/license/reputation/conditions information, fleet sorting/groups and ship loadout/weapons/cargo inspection/reordering/jettison. |
| [ ] | UI-05 | N | Job/active mission lists, destination/deadline/capacity cues, accept/abort and markers with coherent refresh. |
| [ ] | UI-06 | N | Persistent logbook/special logs, message history/categories, notifications/news, first-use help and tooltips. |
| [ ] | UI-07 | P | Rebindable keyboard and mouse UI/aim/select, modifiers, simultaneous inputs, focus/repeat/edge safety and every native gameplay command; IRQ held/edge input is a start. |
| [ ] | UI-08 | N | Settings save/load: gameplay assists, ammo/collection/repair preferences, camera/zoom/text/readability, pause/fast-forward and volume; test long lists and dialogs at target resolution. |
| [ ] | UI-09 | N | Text input, substitutions/names, wrapping/scrolling/font coverage and appropriate glyph conversion; no truncation of consequential choices. |
| [ ] | UI-10 | D | Platform-specific preferences (GPU/vsync/window themes/plugin UI) get an explicit DOS equivalent or inapplicable disposition, not a hidden gameplay omission. |

## H. Assets, audio, data and delivery

Source: `GameData`, `UniverseObjects`, `DataFile`, `DataNode`, `PluginManager`,
`image/`, `audio/Audio`, `audio/SoundCategory`, `PreferencesPanel`, `copyright`;
DOS [brief](../Wishlist.md), [equivalence contract](EQUIVALENCE.md) and existing probes.

| Check | ID | DOS | Feature and proof required |
|---|---|---|---|
| [ ] | SYS-01 | P | All required stock data/assets converted/indexed with reproducible hashes and correct references; raw pack + typed world cover only part of runtime semantics. |
| [ ] | SYS-02 | P | Full ship/planet/station/landscape/UI rendering, animation/rotation/scale, faction recoloring, cloak/weapon/effect visibility; palette and small sprite sample are qualified. |
| [ ] | SYS-03 | N | Sound effects, loop lifetime, positional attenuation/pan, pause/fast-forward and category volume or documented equivalent; Sound Blaster setup/fallback. |
| [ ] | SYS-04 | D | Music/context changes/mute and affordable playback through MIDI/OPL3/MPU-401 or another measured choice; current silence is not completed audio parity. |
| [ ] | SYS-05 | D | Plugin discovery/settings/dependencies/order/overrides/zip/assets and removed-content save behavior: retain stock semantics; general third-party compatibility boundary remains to be chosen. |
| [ ] | SYS-06 | P | Reproducible complete DOS package, installer/paths/config/error reporting, licenses/notices/source, clean-machine launch and conventional/extended memory behavior. |
| [ ] | SYS-07 | P | Full game fits 16 MiB without swap; bounded loading/streaming/caches and mature-save/large-battle peak measurement; microbenchmark heap is not whole-game memory. |
| [ ] | SYS-08 | P | 20,000-cycle target, 800×600 and 30 FPS goal with stable simulation timing; standard/busy/battle routes, percentiles and outcomes, audio enabled; DOSBox cycles are not MHz. |
| [ ] | SYS-09 | N | Standalone standard/busy/battle benchmark, headless report and watch/demo routes; repeatable state/seed/entity counts, not render-only FPS claims. |
| [ ] | SYS-10 | P | Native differential oracles, DOS integration and human campaign tests, crash/error recovery and reproducible issue evidence; existing kernel gates are the starting suite. |

## Explicit adaptation and scope register

The original brief permits cheaper equivalent logic and investigation of reduced
visual/audio cost. Indexed color/software rendering, DOS layouts and equivalent
sound/music delivery do not need to mimic OpenGL internals. Preserve consequential
cues and gameplay rules. Crowd reduction or altered economy/event rates remain
possible concessions requiring evidence and explicit acceptance; they are not
already authorized omissions from beta parity.

Decisions still open: native save interchange (LIFE-06), general third-party
plugin support (SYS-05), affordable audio presentation (SYS-03/04), and exact
full-game performance acceptance envelopes (SYS-07/08). Source-package metadata
and unsupported platform toggles are not gameplay features. Record each decision
here with rationale/evidence; a D row remains unresolved until that happens.

A trainer soundtrack or other original extras remain optional post-beta flair;
use only work needed to validate the port before then. Existing trainers remain
useful development tools. Keep original story content, balance and progression
as the baseline, rather than quietly substituting a smaller new game.

## Beta release gate

- [ ] All in-scope rows have linked completion evidence; decisions resolved and
  any user-accepted departures collected in a release-visible compatibility table.
- [ ] All vanilla files and resolved references validated; every diagnostic triaged;
  a maintained feature/schema coverage matrix and campaign route suite pass.
- [ ] New pilot → earning money → upgrades/fleet → travel/combat/story progression
  → repeated save/reload → major campaign completion works in DOS end to end.
- [ ] Secondary content families and failure/branch cases have regression evidence;
  a single successful story route does not close the whole campaign requirement.
- [ ] Correctness/resource/performance results refer to the same release candidate;
  long-session, large-fleet, battle, audio and interrupted-save cases pass.
- [ ] Human acceptance, reproducible clean install/build, notices/source delivery
  and known limitations are recorded. No open defect prevents normal progression
  or causes unexplained save loss.
