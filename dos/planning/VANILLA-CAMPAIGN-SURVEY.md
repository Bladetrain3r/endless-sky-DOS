# Endless Sky DOS — vanilla campaign/data parity survey

Reviewed by root 2026-09-30; supporting [the beta checklist](../VANILLA-PARITY.md).
Paths below are relative to the repository root.

Audit basis: upstream checkout `061a9461a93898fb691504536589d1dcddc5d79b`
(0.11.4), inspected 2026-09-30. This is a source/data inventory and candidate
beta acceptance checklist. It does **not** claim these features are ported. The
current DOS handover explicitly says cargo, fuel, trade, outfitting, missions,
and saves are absent. “Beta” here means the user's requested vanilla-experience
parity milestone; new flair belongs after that gate and through 1.0.

## Scale and interpretation

The pinned `data/` tree has 204 `.txt` files, 262,132 lines, and 10,500,260
bytes. A lexical top-level-header scan finds 2,344 `mission`, 439 `event`, 938
`outfit`, 917 `ship`, 619 `planet`, 694 `system`, 102 `outfitter`, 73
`shipyard`, 54 named `conversation`, and 5 `start` declarations. These are raw
declarations: variants, extensions/overrides, and deprecated material mean they
are neither unique live-object counts nor proof of semantic coverage. Inline
mission conversations are also not represented by the 54 named-conversation
count. Reproduce raw counts with `python3 dos/tools/feature_inventory.py`; the checked-in
`dos/reports/vanilla-feature-inventory.json` binds all 204 source files by hash.

The existing DOS world pack covers resolved map/orbit/trade data, not this
runtime layer. Vanilla begins with loaded object registries, then applies
per-player event changes and the saved mutable economy. Conditions also expose
live player, fleet, location, reputation, plugin, route, and gamerule state.
Therefore a read-only startup snapshot cannot by itself satisfy campaign parity.

## Beta parity checklist

Each group below names the upstream behavioral anchors and candidate acceptance
cases. Passing only a parser fixture is insufficient where gameplay state spans
multiple subsystems.

### 1. New game and campaign initialization

Evidence: `source/StartConditions.{h,cpp}`; `source/PlayerInfo.cpp` symbols
`PlayerInfo::New`, `RegisterDerivedConditions`, `CreateMissions`; `source/GameData.cpp`
`GameData::StartOptions`; `data/starts.txt` plus four other top-level `start`
declarations in the tree.

- [ ] Load all valid vanilla start declarations, retain selection requirements,
  initial date/system/planet/account/conditions/ships, and quarantine invalid starts.
- [ ] Starting a game applies conditions before ship instantiation and creates
  first-day missions; dated stock events are scheduled.
- [ ] Acceptance: compare native and DOS normalized state for every valid start:
  location/date, credits/debt, fleet/loadouts, condition values, scheduled events,
  and initial offers. Include an unavailable/invalid-start fixture.

### 2. Condition language and live condition providers

Evidence: `source/ConditionSet.cpp` (`Load`, `Test`, `Evaluate`, parser helpers),
`source/ConditionAssignments.cpp` (`Load`, `Apply`), `source/ConditionsStore.*`,
and `PlayerInfo::RegisterDerivedConditions`. Derived providers include ownership
by location, ships and flagship attributes, current/previous planet/system,
visited state, landing access, route length, reputation/enemy state, random/roll,
scheduled-event delay, installed plugins, gamerules, and global/pilot scopes.

- [ ] Parse/evaluate boolean and numeric expressions with vanilla precedence,
  grouping, comparisons, arithmetic/functions, named/prefixed providers, and invalid
  expression behavior; apply all assignment operators with vanilla integer behavior.
- [ ] Preserve the distinction between stored primary conditions and computed or
  externally backed derived conditions; writes to reputation/global/pilot scopes
  must reach the owning store.
- [ ] Acceptance: differential corpus over conditions extracted from all vanilla
  missions/events/conversations, plus boundary values and malformed input. Re-run
  representative conditions after trading, outfitting, parking a ship, traveling,
  installing/disabling a plugin, and save/reload.

### 3. Mission templates, offers, and lifecycle

Evidence: `source/Mission.{h,cpp}` symbols `Load`, `Instantiate`, `CanOffer`,
`CanAccept`, `HasSpace`, `CanComplete`, `CheckDeadline`, `Do`, `StepTimers`, and
`Do(const ShipEvent&)`; `source/PlayerInfo.cpp` `CreateMissions`, `MissionToOffer`,
`AcceptJob`, `MissionCallback`, `StepMissions`, `StepMissionTimers`,
`RemoveMission`; `source/MissionPanel.*`.

- [ ] Support all offer locations: spaceport, landing, job, assisting, boarding,
  shipyard, outfitter, job board, entering, and transition.
- [ ] Preserve offer priority/non-blocking/minor/precedence, repeat/random
  instantiation, substitutions, source/destination filters, waypoints, stopovers,
  marked/tracked systems, clearance/infiltration, visibility, and autosave advice.
- [ ] Preserve cargo/passenger capacity checks, deadlines, apparent vs actual
  payment, contraband/fines/stealth discovery, NPCs, timers, and mission response
  to ship events.
- [ ] Execute all lifecycle triggers: complete, offer, accept, decline, fail,
  abort, defer, visit, stopover, waypoint, daily, and disabled; include `on enter`
  and `on land` actions.
- [ ] Acceptance: deterministic native/DOS trace set spanning each offer location
  and trigger, a randomized repeatable job, impossible capacity, overdue deadline,
  secret landing clearance, waypoint/stopover order, NPC destroyed/captured outcome,
  abort on takeoff for undeliverable mission cargo/passengers, and failure recovery.
- [ ] Content gate: parse and validate every raw vanilla mission declaration with
  zero unexplained warnings; then execute a curated path through each campaign
  family. Header-count coverage alone is not campaign coverage.

### 4. Conversations and choices

Evidence: `source/Conversation.{h,cpp}` (`Load`, `Instantiate`, `Validate`, node/
choice/condition/action traversal); `source/ConversationPanel.*`; callbacks in
`PlayerInfo::MissionCallback` and `BasicCallback`; named and inline conversation
data throughout `data/`.

- [ ] Render paragraphs, scenes, choices, disabled conditional choices, branches,
  labels/gotos, automatic nodes, name prompt, and text substitutions.
- [ ] Preserve special outcomes and their owning flow (accept, decline, defer,
  launch/flee/die/explode where defined), and execute embedded `GameAction`s once
  at the correct node.
- [ ] Acceptance: differential traces for linear, branching, loop/goto,
  all-options-disabled, condition-changing, named-reference, inline, scene, and
  terminal-outcome conversations. Verify keyboard-only choice navigation at DOS
  resolution and save immediately after an action-bearing branch.

### 5. Actions, events, and mutable universe overlay

Evidence: `source/GameAction.cpp` (`LoadSingle`, `Do`, `Instantiate`);
`source/MissionAction.cpp::CanBeDone`; `source/GameEvent.cpp`
(`DeferredDefinitions`, `Load`, `IsValid`, `Apply`, `SaveRawChanges`);
`source/UniverseObjects.cpp` (`Change`, `FinishLoading`, `CheckReferences`);
`source/PlayerInfo.cpp` `AddEvent`, `TriggerEvent`, `AddChanges`, `ApplyChanges`.

- [ ] Actions cover logs/special logs, outfit and ship give/take, payments, fines,
  debt, music/mute, messages, condition assignments, map mark/unmark, mission fail,
  and immediate or delayed events. Honor action location and required-outfit gates.
- [ ] Events apply condition changes and visit/unvisit changes, then batch universe
  mutations. Support changes to fleets, galaxies, governments, outfitters, planets,
  shipyards, systems, news, links/unlinks, substitutions, wormholes, and nested events.
- [ ] Preserve named-event serialization versus unnamed/raw-change serialization,
  deferred definitions, disabled missions/events/persons, and reference validation.
- [ ] Acceptance: one chained scenario must demonstrate conversation → action →
  delayed event → changed system/planet/shop → save → reload with exactly-once
  effects. Separate cases cover two same-day events touching one object, link/unlink
  route recache, removed-plugin deferred object, and disabled content.

### 6. Planet access and port loop

Evidence: `source/Planet.cpp` (`HasServices`, `HasShipyard`, `HasOutfitter`,
`HasFuelFor`, `CanBribe`, `CanLand`, `CanUseServices`, `DemandTribute`);
`source/Port.*`; `source/PlanetPanel.cpp`; `PlayerInfo::Land` and `TakeOff`.

- [ ] Present planet description/landscape/music and the service mix for trading,
  bank, spaceport/news/missions, hiring, outfitter, and shipyard where available.
- [ ] Enforce government reputation, restriction/required attributes, bribes,
  mission clearance versus infiltration, domination/tribute, security fines, and
  per-port fuel/shield/hull/energy recharge and crew hiring.
- [ ] Landing pools local active-fleet cargo, applies fines and mission transitions,
  creates offers, and freezes space flight. Takeoff recharges only allowed ships,
  redistributes cargo/passengers, handles carriers, and aborts impossible missions.
- [ ] Acceptance: native/DOS state traces at an unrestricted full-service world,
  uninhabited/no-service body, hostile bribeable world, mission-clearance world,
  infiltration case, dominated world, and partial-recharge port.

### 7. Trading and dynamic economy

Evidence: `source/TradingPanel.cpp::Buy` and `SellOutfitsOrMinables`;
`source/GameData.cpp` `ReadEconomy`, `WriteEconomy`, `AddPurchase`, `StepEconomy`;
`source/System.*` trade/supply methods; `source/CargoHold.*`; ten commodities are
present in the existing typed world snapshot.

- [ ] Buy/sell bounded by credits, pooled capacity, holdings, and valid price;
  preserve quantity modifiers, commodity cost basis/profit, plundered outfit and
  minable sales, and depreciation.
- [ ] Defer player purchase effects until the economy step, then generate/local-use
  goods and exchange exports over links in vanilla order. Persist both pending
  purchases and every system's commodity supply.
- [ ] Acceptance: seeded multi-system 30-day native/DOS supply/price trace with
  no player activity, purchases, sales, a topology-changing event, save/reload
  mid-run, and a removed-plugin system retained in the purchases ledger.

### 8. Accounts and daily costs

Evidence: `source/Account.cpp` (`Step`, `PayExtra`, `AddMortgage`, `AddFine`,
`AddDebt`, `Prequalify`, `NetWorth`, `CreditScore`); `source/BankPanel.*`;
`PlayerInfo::AdvanceDate`, `MaintenanceAndReturns`, `DoAccounting`.

- [ ] Credits, salary income/back pay, maintenance arrears, mortgages/debts,
  interest/term/payment priority, extra principal, credit score/history,
  prequalification, fleet income/costs, and tribute survive daily advancement.
- [ ] Acceptance: native/DOS daily ledger traces for solvent and insolvent fleets,
  multiple debt kinds, partial salary/maintenance payment, payoff, mission fine/debt,
  and save/reload before and after a native day-advance boundary (not wall-clock midnight).

### 9. Cargo, passengers, contraband, and storage

Evidence: `source/CargoHold.cpp` (`Transfer*`, `Add/RemoveMissionCargo`, `Value`,
`IllegalCargoFine`, `IllegalPassengersFine`); `PlayerInfo::PoolCargo`,
`DistributeCargo`, `Storage`, `AdjustBasis`; `source/BoardingPanel.*` for plunder.

- [ ] Distinguish commodities, outfits/minables, mission cargo keyed by mission,
  passengers keyed by mission, bunks, ship holds, pooled landed cargo, and
  per-planet outfit storage. Preserve fractional outfit mass behavior.
- [ ] Acceptance: multi-ship redistribution with parked/disabled/remote ships,
  flagship priority settings, carrier craft, insufficient hold/bunks, contraband
  scan/fine conversation, boarding plunder, storage transfer, and reload. Verify
  mission cargo identity by UUID rather than display name.

### 10. Outfitting

Evidence: `source/OutfitterPanel.cpp` (`CanMoveOutfit`, `MoveOutfit`,
`ShipCanAdd`, `ShipCanRemove`, `HasLicense`, `CheckRefill`, `Refill`);
`source/ShopPanel.cpp` (`CheckForMissions`, `LicenseCost`); `source/Outfit.*`,
`source/Ship::AddOutfit`; 938 raw outfit declarations and 102 outfitter declarations.

- [ ] Move items among shop, selected ships, cargo, and planetary storage;
  include multi-ship quantities, sold-back local stock, licenses, depreciation,
  ammo refill, capacity/category/required-attribute constraints, and flight checks.
- [ ] Outfitter-open missions and condition changes must refresh inventory without
  corrupting the transaction; ships that cannot fly must be caught before launch.
- [ ] Acceptance: curated matrix for engines, steering, guns/turrets/ammo, bays,
  massless outfits, negative capacity, licenses, stored/plundered gear, refill,
  multi-select, sell/rebuy, mission-granted outfit, and save/reload exact loadouts.

### 11. Shipyard and fleet management

Evidence: `source/ShipyardPanel.cpp` (`CanDoBuyButton`, `BuyShip`, `Sell`);
`source/PlayerInfo.cpp` `BuyShip`, `GiftShip`, `SellShip`, `TakeShip`, `DisownShip`,
`ParkShip`, `RenameShip`, `ReorderShip`, `SetFlagship`; `source/ShipManager.cpp`;
`source/Ship::Save/Load`; 917 raw ship and 73 shipyard declarations.

- [ ] Buy/sell chassis versus ship+outfits, license costs, depreciation, naming,
  flagship selection, fleet order/groups, parking, remote/disabled/destroyed state,
  carrier relationships, fleet capacity/cost, and mission gift/take/disown semantics.
- [ ] Acceptance: buy, customize, rename, reorder/group, park remotely, switch
  flagship, carry a fighter, sell chassis with stored outfits, capture/gift/take a
  ship by ID, and reload. Compare money, depreciation, UUIDs, locations, cargo,
  parent links, outfits, crew, damage, and groups.

### 12. Savegames, snapshots, and transactional safety

Evidence: `source/PlayerInfo.cpp` `Load`, `Save`, `Autosave`, `StartTransaction`,
`FinishTransaction`, `ValidateLoad`; `source/SavedGame.*`; `source/LoadPanel.cpp`
snapshot/load/delete flows. The save includes player/UI location, fleet, storage,
licenses/account/cargo/basis/stock/depreciation, active and available missions,
off-world mission allocation keyed by mission+ship UUID, conditions, gifted ships,
scheduled events, raw changes, mutable economy, destroyed persons, exploration,
logs, start data, enabled-plugin names, and optional messages.

- [ ] Preserve save eligibility: `PlayerInfo::CanBeSaved` requires a live, unlocked
  pilot with planet/system/file path. Arbitrary in-flight flagship saving is not a
  vanilla parity requirement. Round-trip every owned/done/known category above, snapshots (`~` handling),
  autosave policy, launch/cloak/clearance state, and transaction rollback semantics.
- [ ] Loading applies saved universe changes before validation/finalizing ships;
  missing plugin definitions pause missions and report invalid events without
  silently deleting serialized state. Unknown legacy fields should follow native
  tolerance/warning behavior.
- [ ] Acceptance: normalized native→native, DOS→DOS, and cross-runtime round trips if native save interchange is adopted (separate scope decision),
  alongside required DOS persistence cases for fresh start, landed mature pilot with remote/in-flight escorts, active campaign,
  pending events, mutated universe/economy, snapshot, and intentionally removed
  plugin. Byte identity is not required; semantic state and next-step behavior are.
- [ ] Crash/power-loss case: interrupt a save and prove the last good pilot remains
  loadable. A DOS save format or atomic-write design is not yet qualified.

### 13. Vanilla content and plugin compatibility

Vanilla loader semantics used by stock content are beta requirements. General
third-party plugin installation/zip/shader compatibility is surveyed here, but its
DOS support boundary needs an explicit decision; it is not silently counted done.

Evidence: `source/GameData.cpp::LoadSources`; `source/UniverseObjects::Load`,
`LoadFile`, `FinishLoading`, `CheckReferences`; `source/PluginManager.cpp` (`Load`,
`LoadSettings`, `IsPlugin`, `TogglePlugin`); `source/Plugin.*`; derived condition
`installed plugin: ` in `PlayerInfo::RegisterDerivedConditions`.

- [ ] Reproduce the observed source order: core resources, global plugin
  directories, global plugin zips, user plugin directories, then user plugin zips
  (`GameData::LoadSources`). Preserve cumulative duplicate-definition behavior and
  the root-level `overwrite` directive in `UniverseObjects::LoadFile`; qualify exact
  precedence with an executable fixture rather than assuming generic last-file wins.
- [ ] Parse `plugin.txt` name/about/version/authors/tags and game-version,
  required/optional/conflicting dependencies; persist enabled-next-restart state,
  reject duplicate names/invalid dependency sets, and expose installed-plugin
  conditions. A folder is a plugin if it has data/images/shaders/sounds.
- [ ] Support additive/overriding object declarations, references defined later,
  explicit disables for missions/events/persons, and plugin removal/reinstall with
  inactive mission recovery. Do not reduce compatibility to “loads extra files.”
- [ ] Acceptance plugin suite: new isolated content; override/extend stock objects;
  dependency/conflict metadata; duplicate name; disabled content; event-created
  deferred objects; mission/save using plugin ships/outfits; disable, load degraded,
  re-enable, and recover. Record deliberate exclusions such as zip support rather
  than calling partial loading plugin compatible.

## Cross-cutting beta exit gates

- [ ] Pin source and vanilla data hash in every parity report; no moving upstream
  target during qualification.
- [ ] Build a native oracle that emits normalized state transitions, not screenshots
  alone. DOS and native must use the same seeded choices and dates.
- [ ] Full static validation: every vanilla data file parsed, all managed references
  checked, and every warning classified. Preserve a count of live valid objects after
  resolution in addition to the raw header inventory above.
- [ ] Full campaign smoke: new game through a representative main campaign ending,
  with side jobs, boarding, ship purchase/outfitting, economy progression, and
  repeated save/reload. Curated paths should collectively touch every mission offer
  location, lifecycle trigger, conversation node kind, action kind, and event-change
  object kind.
- [ ] Persistence oracle: after every accepted mission, completed mission, event,
  major purchase, and day advance in the smoke path, reload and compare the next
  observable transition as well as stored fields.
- [ ] DOS resource qualification must measure resident/peak memory and disk size on
  the complete resolved vanilla campaign dataset plus a mature save; the existing
  16 MiB flight/world results do not establish this.
- [ ] Human playtest at 800×600 covers readable mission log, long dialogs/choices,
  job board sorting/deadlines, shop/fleet workflows, and load/save recovery using
  keyboard controls.
- [ ] Track every deliberate departure from pinned vanilla with user-visible impact
  and an acceptance rationale. New content or flair does not substitute for an
  unimplemented vanilla campaign mechanic before beta.

## Schema and implementation traps worth keeping visible

- Mission instances embed randomized/resolved data and UUID identity; reparsing the
  template on load is not equivalent.
- Conditions are a language plus a dynamic provider registry. Caching their values
  as ordinary flags breaks offers after location, fleet, route, reputation, plugin,
  or gamerule changes.
- Events form a persistent mutable overlay on initial content. Named events may save
  by name; unnamed events and `save raw changes` require raw mutation preservation.
- Economy persistence uses commodity header order plus per-system supplies and a
  separate pending-purchases block. Commodity-set changes and removed-plugin systems
  are compatibility cases.
- Mission cargo/passengers stored off-world use both mission and ship UUIDs; ordinary
  `CargoHold::Save` intentionally omits mission cargo because missions reconstruct it.
- Plugin/object loading is cumulative and order-sensitive, and a root `overwrite`
  node clears the next object's prior definition for supported types. Raw declaration
  counts therefore overcount live objects. Undefined references may be placeholders,
  deferred event definitions, invalid content, or removed-plugin state and need
  different handling.
- Saved enabled-plugin names are diagnostic; actual content availability comes from
  startup source discovery/settings. A save mentioning a plugin does not load it.
