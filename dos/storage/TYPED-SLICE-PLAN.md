# First typed content slice: resolved map and starting economy

Status: proposed implementation contract, 2026-09-27. No typed pack or DOS
world loader exists yet. Use the indexed pack selected in [README.md](README.md).

## Choice and boundary

Build one **vanilla, read-only snapshot** of systems, planets, stellar-object
orbits, links, and starting trade prices. This gives the next menu/map/flight
prototype real connected locations and a compact lookup target. Defer ships and
outfits: `UniverseObjects::FinishLoading()` finishes ships only after outfits,
and `PrintData`'s ship statistics include derived installed-outfit values
(`source/UniverseObjects.cpp:87-105`, `source/PrintData.cpp:160-250`). Also defer
landing services, shops, government behavior, fleet spawning, descriptions,
economy ticks, and event-driven universe changes. The snapshot must not be
claimed as a complete playable world.

Build-time extraction should run **after the native game's normal data load and
`UniverseObjects::FinishLoading()`**, using `GameData::{Systems,Planets,
Commodities}` and public object getters. Add a native-only dump command/tool for
fields absent from `PrintData`; leave the existing baseline binary and source
revision as the comparison control. Do not compile raw text blocks directly into
records: `DataFile` tokenizes quotes, comments and indentation; `Set::Get` creates
forward-reference placeholders; `UniverseObjects::LoadFile` applies ordered
definitions, top-level `overwrite`, and plugins; `System::Load` and `Planet::Load`
apply per-field add/remove/first-occurrence rules (`source/DataFile.cpp:97-210`,
`source/Set.h:26-33`, `source/UniverseObjects.cpp:40-76,369-390,530-585`,
`source/System.cpp:90-210`, `source/Planet.cpp:66-155`). Export the final native
state, not a second interpretation of these rules. Fix the source revision,
resource tree, enabled-plugin list/order and config in the pack manifest; the
first acceptance gate uses vanilla data with plugins disabled.

## Exact first schema

Versioned little-endian pack tables, with explicit counts/offsets and bounds:

| Table | Fields to export |
| --- | --- |
| Identity | kind (`system`/`planet`/`commodity`), stable ID, exact UTF-8 key bytes, defined/valid flag; include referenced placeholders, so unresolved references remain distinguishable from missing IDs. |
| System | ID, display-name string, map `x`,`y` as binary64, `IsValid`, `Hidden`, `Shrouded`, `Inaccessible`, `JumpRange`, two arrival and two departure distances, `HabitableZone`, ordered attribute IDs; preserve even invalid system rows for reference diagnostics. |
| Link | source-system ID, target-system ID for each **accessible** `System::Links()` edge; separately record declared raw-link targets only if a native extractor exposes them, since `Links()` filters inaccessible or invalid targets during `UpdateSystem`. The first runtime routes on accessible links. |
| Planet | ID, display-name string, `IsValid`, `IsInhabited`, ordered attribute IDs, list of system IDs from `Planet::Systems()`; preserve multi-system membership (wormholes). No landability claim follows from these flags alone. |
| Stellar object | owner-system ID, native object `Index`, `Parent` index (-1 root), optional planet ID, optional sprite-name string, `Distance`, `Period`, `Offset` as binary64. Keep native vector order and parent indices; the record is for map/orbit lookup, not image residency or collision masks. |
| Commodity | ID, native list ordinal, exact name, `low`, `high`; omit special commodity item lists from this first runtime slice. |
| Initial trade | system ID, commodity ID, integer `System::Trade(name)` for each defined price; distinguish absent entry from price zero, because `Trade()` returns zero for an absent name. A native dump must expose `HasTrade`/raw trade presence or the builder must derive presence from resolved native data with a documented API addition. |

The `System` flags and fields above come from `source/System.h:87-172`; orbit
fields from `source/StellarObject.h:48-88`; planet membership from
`source/Planet.h:91-145`; commodity fields from `source/Trade.h:28-51`.
`System::UpdateSystem` computes accessible links, default habitable distance,
orbital periods and the automatic `uninhabited` attribute, so extract afterward
(`source/System.cpp:537-683`). The initial trade price is set by
`System::Price::SetBase`; later `StepEconomy` is mutable and outside this slice
(`source/System.cpp:1297-1308`).

Use exact, case-sensitive UTF-8 names as identity inputs. Assign a namespaced
64-bit ID from a documented cryptographic digest of `(kind, length, name)`;
reserve zero, reject any digest collision at build time, and keep a sorted
ID-to-name table for diagnostics. Never use raw-block manifest positions, native
pointer addresses, map traversal ordinal or pack offsets as persistent IDs.
Saves should retain names (or name plus ID) and the content-manifest hash;
re-resolve IDs when the content set changes. Object indices are local to their
system's current snapshot, not persistent identities. A future event that
replaces a system requires rebuilding its runtime record and links; this pack
does not silently freeze event semantics.

## Verification and memory gate

`PrintData` already provides independent, post-load oracles: `--systems`,
`--systems --attributes`, `--planets`, `--planets --attributes`, and
`--orbits --all` (or `--orbits --systems <name>`). Its output is emitted after
`GameData::BeginLoad(...).wait()` in console mode (`source/main.cpp:160-195`;
`source/PrintData.cpp:603-731,836-883`). Compare parsed sets and orbit trees,
including empty/undefined rows, with the generated pack. `--orbits` prints
habitable distance and derived periods, useful numerical checks. It does **not**
print links, map positions, all flags, planet membership or trade prices, so
compare those against a separately built native dump from getters; do not infer
their correctness from the PrintData checks. Compare binary64 values exactly
when both exports read the same in-memory getters; if comparing PrintData's
decimal text, use an explicit half-last-printed-unit tolerance.

Add tiny synthetic data fixtures that exercise a link and planet reference before
definition, a later definition, `overwrite`, repeated system/planet definitions,
`add`/`remove`, an inaccessible link, an orbital child, a multi-system planet,
and repeated trade definitions. Run the unchanged native loader and candidate
extractor against each fixture. Validate every table offset/length and reference,
reject duplicate IDs/collisions/unknown required IDs, and mutate a pack copy to
prove the DOS reader rejects truncation and bad references. Finally compare a
vanilla pack's every row to the native dump and run lookup/iteration under
16 MiB, swap disabled, 20k DOSBox cycles. Record guest memory and lookup batch
time separately from host RSS and rendering FPS.

Resident budget for this milestone: at most **256 KiB tracked heap** for typed
content access, including a name/ID directory (target <=128 KiB), two 32 KiB
record pages, and <=64 KiB scratch/index overhead. Keep strings and orbit/trade
arrays in the pack, page them by offset, and hold only the active system plus
its immediate neighbors and object records as decoded runtime state. Fail the
build if the directory cannot meet the target; report actual allocator high-water
and DOS free-page observations. This is an access-layer budget, not a full-game
16 MiB proof. The existing raw-block experiment used about220 KiB of explicitly
tracked pack allocations (`dos/storage/README.md`), a useful bound rather than
a guarantee for typed objects.
