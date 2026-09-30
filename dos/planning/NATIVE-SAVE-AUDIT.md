# Native persistence contract audit for ESDOS-002

Audit date: 2026-09-30. Source inspected read-only in
`/data/NuCode/Codex/endless-sky-DOS` at local Git revision
`bb6b98233d032c109c7eca674c9b0e754c94abfc` (the project records upstream
reference `061a9461a93898fb691504536589d1dcddc5d79b`). Source audit by a bounded Sol delegate, reviewed against source by Astra. The
audit itself ran no executable or user save. Statements marked **observed** below
are source findings; the proposed acceptance matrix remains broader than the
[subsequent executed fixture](../persistence/README.md), whose exact scope and
results are recorded separately.

## Scope decision: DOS save/load versus LIFE-06

**Observed.** The parity ledger makes DOS persistence mandatory in LIFE-02--04,
but marks native import/export and its supported version range as a distinct,
still-open LIFE-06 decision (`dos/VANILLA-PARITY.md:56-61,217-221`). ESDOS-002
likewise says native interchange is a separate proposal (`dos/TICKETS.md:30-45`).
Therefore the bounded contract should use native as a behavioral oracle and
fixture producer, while defining a versioned DOS-owned campaign save. It should
not promise that either runtime can consume the other's files. Exact native text
interchange would substantially enlarge the work because native persistence is a
multi-file, content-name-dependent format with no explicit save-format version.

An in-flight replay/checkpoint is also a separate artifact. Native campaign saves
are eligible only while landed and omit transient flight state such as velocity,
facing, energy, heat, commands, projectile pools, targets, and RNG state. A replay
checkpoint may capture those for deterministic test continuation, but must not be
accepted as a campaign save or counted toward LIFE-02--04.

## Eligibility, save destinations, and transaction meaning

**Observed.** `PlayerInfo::CanBeSaved()` is exactly: player is not dead, player
planet exists, player system exists, save path is nonempty, and the pilot profile
is not locked (`source/PlayerInfo.cpp:5600-5604`). `Save()` returns without doing
anything if this predicate fails (`source/PlayerInfo.cpp:639-644`). `Autosave()`
adds the further requirements that the path has at least four characters and the
pilot does not use `single save file`; it writes `~autosave.txt`
(`source/PlayerInfo.cpp:4987-4994`). The normal save records `recent.txt` first,
rotates dated `~~previous-N` files only when the in-file date changed, optionally
writes a `~~previous-spaceport` snapshot, then writes the primary save, pilot
profile, and global conditions (`source/PlayerInfo.cpp:646-685`). Taking off calls
save before clearing landed state (`source/PlanetPanel.cpp:587-592`); orderly quit
also saves only while landed and when the UI stack permits saving
(`source/main.cpp:638-640`).

`StartTransaction()` serializes the whole `PlayerInfo` into an in-memory
`DataWriter`; while it exists, any path save writes that frozen serialization
instead of current state. `FinishTransaction()` merely discards the snapshot
(`source/PlayerInfo.cpp:729-744,4998-5007`). This is **persistence isolation**, not
a live-memory rollback or general atomic transaction. Conversations start it to
prevent partial effects from reaching a save and finish it on exit
(`source/ConversationPanel.cpp:90-96,490-495`). The checked-in integration test
observes that a reload during a conversation restores pre-conversation conditions,
then a save after conversation exit retains completed effects
(`tests/integration/config/plugins/integration-tests/data/tests/tests_save_load.txt:86-142`).
Commodity trading does not call this mechanism: the buy path adjusts basis, cargo,
credits, and economy tracking in sequence (`source/TradingPanel.cpp:303-333`).

Contract consequence: an ESDOS landed transaction should have an explicit commit
boundary and preserve native quantity clamping for insufficient credits, hold
space or holdings; zero available quantity means no economic change. Its durable save should expose only a fully committed before or after
state. Do not describe native `StartTransaction` as providing rollback of a failed
trade.

## Serialization ownership and exact load order

**Observed.** A native pilot is not one self-contained file:

- `saves/<identifier>.txt` owns per-save player state. `PlayerInfo::Save(DataWriter&)`
  writes identity/date/location and UI state, all owned ships, storage, licenses,
  account, pooled cargo, commodity basis, stock/depreciation, missions and UUID
  allocations, conditions, gifted-ship UUIDs, scheduled events, raw universe
  changes, economy, destroyed persons, discoveries/logs/start data, enabled plugin
  names, and optionally message history (`source/PlayerInfo.cpp:5012-5373`).
- `pilots/<identifier>.txt` owns the pilot lock, moment-of-death summary, pilot-wide
  conditions, and gamerules (`source/PilotProfile.cpp:250-277`; ownership statement
  in `source/PilotProfile.h:36-39,73-77`). Profiles group main and `~snapshot` save
  names by filename prefix (`source/PilotProfile.cpp:38-98`).
- `global conditions.txt` is separately written after the save and pilot profile
  (`source/PlayerInfo.cpp:681-685`). Thus copying only a native save file does not
  reproduce all semantics.

The load sequence is consequential:

1. `PlayerInfo::Load` clears the previous player, reverts mutable `GameData`, resets
   messages, attaches and loads the profile, normalizes a snapshot path back to the
   main `.txt` destination, registers derived conditions, then parses top-level
   records (`source/PlayerInfo.cpp:239-248,303-535`).
2. Ships are constructed during that parse, marked special/player-owned, but final
   loading is deferred (`source/PlayerInfo.cpp:402-410`). Missions load before
   off-world mission cargo is redistributed; the latter is temporarily keyed by
   mission UUID string and ship UUID string (`source/PlayerInfo.cpp:310-315,456-472`).
3. `ApplyChanges()` applies reputations/raw changes, rebuilds systems/wormholes,
   restores economy, installs pilot gamerules and date, restores destroyed persons
   and domination, checks references, and only then calls `Ship::FinishLoading(false)`
   (`source/PlayerInfo.cpp:3798-3845`). This allows mutable content and plugin data
   to affect ship finalization.
4. `ValidateLoad()` repairs missing/invalid player and ship locations, clears an
   invalid travel plan, supplies an old save's start scenario, pauses invalid active
   missions, drops invalid available missions, and records invalid events
   (`source/PlayerInfo.cpp:3850-3964`). These are normalizations, not byte-exact
   round-trip behavior.
5. Mission caches are rebuilt, saved clearance is restored, cargo capacities are
   recalculated, and off-world mission cargo/passengers are redistributed only
   where both UUID lookups succeed. Missing links remain undistributed rather than
   binding by fleet position/name (`source/PlayerInfo.cpp:536-586`). Depreciation,
   original names, and fleet limits receive compatibility defaults afterward
   (`source/PlayerInfo.cpp:588-604`).
6. Constructing the main panel for a landed player opens the planet panel and calls
   `PlayerInfo::Land()` (`source/MainPanel.cpp:95-101`). On this freshly-loaded path
   it visits the planet, removes destroyed/unowned ships, unloads bays, recharges
   eligible ships according to port services or onboard regeneration, places
   eligible escorts, pools landed cargo, and steps mission state; it suppresses
   new fines, NPC updates, and new mission generation until a real subsequent
   landing (`source/PlayerInfo.cpp:1705-1858`).

## Ship identity and normalization

**Observed.** Every serialized ship carries its UUID text (`Ship::Save`,
`source/Ship.cpp:1261-1286`), and `Ship::Load` restores that UUID
(`source/Ship.cpp:478-534`). It also writes a full mutable ship snapshot: model and
names, base attributes, outfits and weapon layout, cargo, crew, fuel/shields/hull,
position, effects/layout, system/planet/destination, and parked flag
(`source/Ship.cpp:1261-1514`). Ship location references and resource values load
directly (`source/Ship.cpp:850-872`), then `FinishLoading(false)` merges omitted
base-model material, rejects/normalizes invalid equipment, recomputes caches and
disabled state, and clears unreachable target systems (`source/Ship.cpp:915-1235`).

This is more than a stable model ID plus mutable overlay: native saves embed much
of the resolved definition. A DOS-owned format can keep immutable definitions in a
hash-bound content pack and mutable state in the save, but equivalence must compare
observable outcomes and surface a missing definition; it must not silently attach
saved state to a different model.

Malformed UUID text logs a warning and the native `EsUuid` lazily generates a new
random ID when its value is next used (`source/EsUuid.cpp:128-191`). Native code in
the inspected path does not show a fleet-wide duplicate-ID rejection. For the DOS
contract, require a syntactically valid, nonzero, unique ship ID; reject duplicate
IDs and preserve the prior save. A missing/deleted ID must become an explicit
unresolved reference, never a search by name, model, or vector position. This is
stricter failure handling in service of the ESDOS-002 invariant, and should be
recorded as such rather than claimed as exact native error behavior.

Important landed normalization: the file writes fuel, shields and hull, but not
energy or heat (`source/Ship.cpp:1363-1368`). On load, `Land()` invokes
`Ship::Recharge`; depending on port recharge flags and onboard generation/repair,
that may refill shields, hull, energy and fuel, always resets heat to `IdleHeat`,
clears status effects and repair delays, and may hire required crew
(`source/PlayerInfo.cpp:1753-1792`; `source/Ship.cpp:3261-3287`). Cargo is pooled
from co-located ships after capacities are derived (`source/PlayerInfo.cpp:1794-1804`).
Therefore acceptance should compare the **post-`Land()` normalized state**, not
expect raw resource fields or per-ship landed cargo placement to survive unchanged.

## Durability findings

**Observed.** `DataWriter` composes text in memory with stream precision 8 and
writes in its destructor or explicit `SaveToPath` (`source/DataWriter.cpp:31-69`).
`Files::Write` opens the destination directly with output/truncation semantics,
writes, and flushes; an unopenable stream is silently ignored and write/flush
errors are not checked (`source/Files.cpp:461-478,499-512`). There is no temporary
file, fsync, checksum, rename-on-success, or readback validation in this layer.
`Files::Move` is a bare rename (`source/Files.cpp:389-392`). On a date-changing
normal save, moving the prior primary to `~~previous-1` gives a recovery copy
before the new primary is written; a same-day overwrite has no such protection
(`source/PlayerInfo.cpp:649-679`). `recent.txt` can point at a missing or partial
primary because it is written first.

Consequently native write behavior is not the ESDOS-002 interrupted-write target.
The DOS contract should require that a short write, media-full result, malformed
candidate, or interrupted replacement leaves the last validated save loadable.
That acceptance property is a deliberate durability improvement, independent of
LIFE-06 text interchange.

## Bounded native purchase fixture and result schema (proposed, unexecuted)

Fixture name: `native-landed-commodity-purchase-v1`. Create it only under an
isolated test config, using checked-in/injected test data rather than a user save.
Pin the native executable revision, data/plugin manifest hash, default gamerules,
system `S`, planet `P`, one unparked flagship with fixed UUID `U`, cargo capacity
`K`, credits `C`, date `D`, and commodity `Q` whose displayed unit price is `Pq`.
Choose integer values with `K-used >= 3` and `C >= 3*Pq`; buy exactly 3 tons, save
while still landed, terminate, reload through the normal main-panel/`Land()` path,
then record the first legal next action (another one-ton buy and its outcome).

Capture one JSON record at four phases: `before`, `after_purchase`,
`serialized_parse`, and `after_reload_normalized`. Required fields:

```
fixture_version, native_revision, content_manifest_sha256, gamerules_name,
pilot_identifier, save_filename, date_days, system_id, planet_id,
can_be_saved, pilot_locked,
credits_i64,
commodity_id, unit_price_i64, pooled_qty_i32, cargo_used_i32,
cargo_capacity_i32, cargo_free_i32, cost_basis_i64,
ship_uuid, ship_model_id, ship_system_id, ship_planet_id, ship_parked,
ship_crew_i32, ship_fuel_f64, ship_shields_f64, ship_hull_f64,
pending_economy_qty_i32,
next_action, next_action_accepted,
warnings[], save_sha256
```

Integer invariants for the accepted buy are:

- `qty_after = qty_before + 3`;
- `credits_after = credits_before - 3 * unit_price`;
- `basis_after = basis_before + 3 * unit_price`;
- `used_after = used_before + 3`, `free_after = capacity - used_after`, and no
  negative credits/free space;
- date, player system/planet, ship UUID/model/location, pilot lock, and gamerules
  are unchanged by the purchase;
- after normal reload, credits, pooled quantity, capacity, cost basis, date,
  location, stable IDs, and the next one-ton purchase result equal the committed
  post-purchase values/outcome;
- resource assertions use the normalized port contract described above. UUIDs and
  all integers are exact; serialized floating values should be compared after
  native parsing and normal landing normalization, not against arbitrary in-memory
  pre-save bits because `DataWriter` uses 8-digit formatting precision.

Native `GameData::AddPurchase` records only negative ton deltas in the inspected
implementation (`source/GameData.cpp:461-465`), while `TradingPanel::Buy` passes a
positive quantity (`source/TradingPanel.cpp:315-333`). Thus this buy fixture should
expect its pending-economy quantity to remain unchanged; it must still capture the
field so a later sell/economy fixture can test the deferred economy path.

Run paired boundary cases from the same baseline: request more than free capacity,
request more than affordable, request an invalid/zero-price commodity, and attempt
save while in flight. Native clamps positive purchase quantity to both free space
and `credits / price` (`source/TradingPanel.cpp:315-331`) and `CargoHold::Add`
clamps again to free capacity (`source/CargoHold.cpp:507-517`); the zero-price path
returns without mutation (`source/TradingPanel.cpp:310-313`). Record native results,
then define DOS acceptance explicitly (exact native clamping or explicit rejection)
rather than letting UI behavior decide persistence semantics accidentally.

Finally, exercise DOS durability separately with candidate truncation at every
record boundary and simulated short/out-of-space writes: restart must load the last
validated generation; no candidate may partially replace it; duplicate/malformed
ship IDs and missing content IDs must surface a deterministic error and must not
bind another entity. These cases validate the DOS save contract. They are not proof
of native save-file interchange or a supported native version range.
