# Persistent pilot and save/load contract

2026-09-30 — ESDOS-002 design checkpoint, authored/reviewed by Astra.
Native baseline: `061a9461a93898fb691504536589d1dcddc5d79b`.
This is the version-1 design contract; its bounded implementation now lives in
[CAMPAIGN.md](../flight/CAMPAIGN.md). Wider campaign and interchange gates remain open. [Native audit](NATIVE-SAVE-AUDIT.md), [fixture](../persistence/README.md),
[work queue](../TICKETS.md), [parity ledger](../VANILLA-PARITY.md).

## Decision and boundaries

Mandatory first: LIFE-02–04, a persistent pilot and reliable landed saves. LIFE-06
is exchange with native Endless Sky; research it now, use native-generated fixtures,
and make any actual interchange a separate, versioned conversion path. Do not
claim native compatibility merely because we reuse its names or text grammar.

Choose a **DOS-owned versioned binary save**, pinned to the content build, with a
small host-side inspection/export tool later. Reuse the proven binary64 transfer
approach for numeric state. Keep native text saves unchanged and outside the DOS
writer's namespace. Native saves embed resolved ship definitions and span save,
pilot-profile and global-condition files; direct text reuse would not save us the
semantic work, and its eight-digit floating output is not our replay format.

Two artifact classes must remain distinct:

| Artifact | Meaning | Contents / loading rule |
|---|---|---|
| Campaign save | Resume a pilot at a legal landed boundary | Persistent state, profile policy, content identity; normalize landing through native-equivalent rules |
| Test checkpoint/replay | Reproduce a known simulation state | Version/seed and all relevant transient state, iteration order/input cursor/ticks/projectiles/resources; never accepted as a campaign save |

A seeded scene plus recorded inputs is sufficient for current flight tests. Do
not build a generic memory-image checkpoint serializer as a side effect of this
first save slice. Later checkpoints need their own version and acceptance gate.

## Source contracts that constrain the implementation

- `PlayerInfo::CanBeSaved`: live player, planet, system, nonempty destination and
  unlocked pilot. Takeoff saves before leaving the planet. In-flight gameplay
  does not create a normal save, and attempting it must leave the prior save intact.
- `PlayerInfo::StartTransaction` freezes the serialization used by saves until
  `FinishTransaction`. It does **not** restore live state on failure. Conversation
  effects remain isolated from intermediate saves; commerce has a separate sequence.
- `TradingPanel::Buy` clamps purchases to free space/affordability, and sales to
  holdings. Zero-price/invalid selection performs no transaction. Preserve these
  native outcomes rather than rejecting every oversized quantity wholesale.
- A purchase changes cargo, credits and cost basis together. Native
  `GameData::AddPurchase` records only negative quantities (sales) in the pending
  economy ledger; a buy does not add a positive pending entry in this revision.
- Load applies saved changes/economy and resolves ships before normal landed
  initialization. `Land` can recharge/hire crew/pool cargo and update mission state;
  compare that normalized result and the next action, not raw file bytes alone.
- Native campaign ships save UUID/model/location/crew/fuel/shields/hull, but not
  a complete flight execution state. Native `EsUuid` ordinary copy intentionally
  clears identity; use explicit identity preservation in comparisons/conversions.
- Native disk writes are direct writes with flush, not a checked transactional
  replacement. Stronger DOS error/recovery behavior is an intentional improvement.

## State owners and identity

| Owner | Persistent responsibilities | References and mutation boundary |
|---|---|---|
| Content store | Immutable stock definitions and lookup tables | Exact content-build manifest hash; native namespace + true name/variant key, never a raw pointer or pack offset |
| Campaign | Date/location, fleet, pooled cargo/basis, accounts, exploration, active mission instances, pending events and mutable universe/economy | Stable pilot/ship/mission instance IDs; all affected fields in one committed campaign generation |
| Ship | Instance identity/name, model/variant, outfit/loadout, crew/resources, cargo/location/parent/park state | UUID identity independent of fleet order and display name; content refs resolved only after validation |
| Pilot profile | Lock/death policy, pilot-wide conditions and gamerules shared across snapshots | Separate scope from rewindable campaign; restoration must not undo a lock or profile-wide progression |
| Installation | Global unlock conditions and preferences shared across pilots | Explicit separate scope; cannot be accidentally overwritten by importing a pilot |
| Runtime | Camera, input state, targets, transient actors/projectiles, frame timers, cache pointers | Reconstructed for campaign load; only a dedicated replay/checkpoint may restore exact transient execution |

Use nonzero 128-bit IDs for pilot and ship instances (native-compatible UUID text
at conversion boundaries); future mission IDs follow the same identity domain.
A copied template gets a new instance ID; save/reload or explicit snapshot clone
preserves identity. Pilot and ship IDs must differ; reject duplicates before linking references. Deleted or absent
IDs never fall back to vector index, display name or another ship of the same model.

Keep immutable definitions separate from a mutable overlay keyed by typed content
identity. Loading a future mission/event must be able to change a planet/shop/link,
then resolve dependent state. Do not bake the current Earth/Luna service snapshot
into the general save model. Derived conditions are recomputed from their owner,
not saved as an unrelated collection of boolean flags.

A growth example (design only): mission instance M owns its accepted/paid state
and references ship U by UUID for delivery cargo. Completing M increments a
campaign condition and schedules event E for date D+2; E changes a named planet
shop in the mutable overlay. Reload must retain M/U/E links, due date, condition
and whether E was already applied, so it cannot award payment or apply E twice.
Profile/global conditions remain separately owned. Version 1 rejects this nonempty
scope; it never drops M/E while retaining a misleading "successful" save.

The first implementation supports one pilot/one ship and default unlocked policy.
It may carry an explicitly empty overlay/mission/event set; it must reject files
requiring unsupported nonempty state. Later snapshot/profile/global stores need
scope-specific transactions and tests before claiming multi-pilot/campaign parity.

## Version 1 envelope and bounded first payload

Normative envelope for ESDOS-003: two alternating files `PILOT0.SAV` and
`PILOT1.SAV` inside a dedicated development-pilot directory. Filenames are 8.3;
player display names never become paths. Do not reuse a native `.txt` destination.

All integers little-endian, serialized field by field; never dump a C struct.

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 8 | Magic ASCII `ESDSAVE1` |
| 8 | 4 | Schema version, initially 1 |
| 12 | 4 | Header bytes, exactly 64 for version 1 |
| 16 | 8 | Nonzero generation; no wraparound |
| 24 | 4 | Payload byte length; maximum 65,536 for first slice |
| 28 | 32 | SHA-256 identity of the exact content manifest used by the runtime |
| 60 | 4 | CRC-32/ISO-HDLC over bytes 0–59 followed by the payload |

CRC is corruption/truncation detection, not authentication. Require exact file
length, known header/schema, a matching content identity and a bounded payload
before publishing loaded state. Check offset/length arithmetic for overflow before
allocation. Little-endian IEEE754 binary64 values must be finite. An unknown or
malformed file remains untouched and yields a named error.

Payload is a sequence of required chunks: `type:u32, version:u32, length:u32,
bytes[length]`. First slice requires exactly one of each chunk below, order
ascending by type, no duplicate or unknown chunks. Unknown future fields are an
explicit unsupported-version error, not silently skipped then lost on save.
Future versions can add an extension policy deliberately.

| Type | Version-1 payload, in order |
|---|---|
| 1: pilot | pilot UUID[16], UTF-8 display name string, date day/month/year i32×3, typed system key string, typed planet key string, flagship UUID[16] |
| 2: ship | ship UUID[16], model key string, variant key string, given name string, crew i32, fuel/shields/hull binary64×3; stock Sparrow loadout only in this slice |
| 3: ledger | credits i64, commodity-entry count u32, sorted entries of commodity key string + tons i32 + total cost basis i64 |
| 4: scope | flags u32=0 (no missions/events/overlay/custom profile/global state), pending-sale count u32, sorted entries of system key string + commodity key string + delta i32 |

String = byte length u16 followed by that many UTF-8 bytes; maximum 255 bytes,
no embedded NUL, valid encoding. Empty variant allowed, other identity keys
nonempty. The ship is at the pilot's location in this single-ship slice; no
remote/parked/carried or custom outfit state can be silently omitted. Model/variant
keys include their namespace in the string (`ship:...`, `system:...`, `planet:...`,
`commodity:...`) so cross-type resolution cannot accidentally succeed.

Maximum 10 commodity entries and 16 pending-sale entries for this initial harness;
unique typed keys, no negative quantities, signed i64 credits/basis, sale deltas
strictly negative. Do checked arithmetic; rejecting unrepresentable input is safer
than signed overflow. Crew/fuel/shields/hull must be within the selected content
model's supported bounds; unavailable required state produces an error, not clipping.
Zero-quantity basis must be zero. Currency remains signed: native Account supports
debt/cost effects; the positive-credit trade fixture does not define all finance.

The header hashes the full pinned build manifest, including model/profile content
and gameplay parameters, not just the raw directory listing. ESDOS-003 must define
and generate that manifest before producing a save. Until wider gameplay exists,
this format is a **development save**, not a promised beta-compatible public format.
Future schema changes require explicit migration or clean rejection.

## Durable commit and recovery protocol

1. Determine eligibility and produce a complete validated candidate from committed
   live state. During an unfinished multi-effect conversation transaction, use the
   last committed snapshot; do not serialize a half-applied change.
2. Inspect both slot files without modifying either. Missing slots are allowed;
   invalid slots are reported. If valid candidates belong to different pilots or
   have equal generations with unequal content, stop with a conflict.
   A structurally intact but unsupported schema/content/scope is not a corrupt
   disposable slot: stop automatic loading/writing and report incompatibility.
   Do not silently select and overwrite an older compatible generation. Explicit
   recovery/export can be a later tool; preserve both files in this case.
3. Choose the highest validated generation as current. Write only the other slot,
   using generation +1. If only one valid slot exists, never truncate that slot.
4. Check every write, flush and close result; request the DOS-supported file commit
   operation and qualify its exact behavior in the runtime. Reopen and validate the
   written bytes/CRC/semantic payload before reporting save success.
5. On any failure, report the error, retain the live dirty state and keep the last
   validated slot as recovery. No “saved” feedback, generation pointer or dirty-bit
   reset until readback passes. Do not automatically retry an economic action.
6. At load/restart, pick the newest fully validated compatible generation. A torn
   inactive slot cannot replace the intact current slot. If recovering the older
   slot, show that recovery happened. With neither valid, fail visibly; never start
   an empty pilot under the old identity.

No rename-over-existing or third mutable selector is needed. Never rewrite both
slots in one save. The first-ever save has no prior generation: interrupted first
creation yields no save, not a fictitious recovered pilot. Reject generation
exhaustion instead of wrapping. External concurrent writers are unsupported;
ESDOS-003 must reject a detected change of generation since the save began.
That check is not a lock or a guarantee against races: this first harness is
single-writer only; concurrent process support needs a separately qualified lock.

This protocol guarantees only what the fault tests establish. Process interruption
and short/media-full writes are testable. Arbitrary host/device power failure can
lose acknowledged cached writes or damage filesystem metadata; DOSBox tests do
not certify real FAT/media/controller durability. Qualify the flush/commit path,
state that limit, and retain export/backup options later.

A future multi-file profile/global-condition update cannot be made atomic simply
by applying this protocol independently to each file. Design a transaction bundle
or ordered recovery protocol before introducing operations that change all scopes.
Do not let loading an older campaign clear an independently committed pilot lock.

## Load sequence and normalization

Read both slots → validate envelope and bounds → parse into a temporary owned
candidate → reject unsupported scope/content/IDs → resolve immutable model/keys
→ validate quantities/capacity/reference graph → normalize the landed boundary
→ swap the fully valid candidate into live state. Any failure leaves the prior
running pilot and source files unchanged. Never partially mutate globals while
parsing untrusted or damaged saved content.

In the bounded scene, restore credits/cargo/basis/date/location/IDs exactly; derive
capacities/mass/drive/resource limits from the pinned stock loadout. Apply the
supported port recharge rules and `IdleHeat` to establish the same post-load
state as native. Put the ship at a reproducible dock state; takeoff uses the
existing seeded placement. Do not serialize framebuffers, caches or raw pointers.

Full campaign load later needs: restore profile/global scope appropriately,
apply universe changes and saved economy, resolve/finish ships, restore mission
instances/UUID allocations, recompute derived conditions and then complete the
landed transition without repeating fines, new offers or already-applied actions.
The initial loader must not claim these semantics from empty scope fields.

## Acceptance contract for ESDOS-003

Files owned by implementation: new `dos/persistence/state.{c,h}`, `codec.{c,h}`,
`store.{c,h}`, `test_*.{c,py}`, a small flight adapter, fixtures and evidence/docs.
Keep native reference source unchanged; keep codec, state and disk I/O separable.
Create the mechanical gates before delegating implementation to a local model.

| Gate | Required result |
|---|---|
| PS-01 codec | Encode/decode the version-1 fixture; exact IDs, integers and binary64 bits; native ASan/UBSan and DJGPP/DOS, no packed-struct dependency |
| PS-02 invalid files | Reject short header/payload, bad CRC, extra bytes, unknown version/chunk, oversized counts/strings, nonfinite numbers, bad/duplicate IDs, unresolved keys and mismatched content; no live mutation |
| PS-03 normal durability | First save, alternating generations, restart, both slot orders and deterministic older-slot recovery; correct error/feedback/dirty state |
| PS-04 injected failures | Interrupt/short-write at every header/chunk boundary and across payload; fail flush/close/commit; old slot remains byte-identical and loadable; no silent success |
| PS-05 conflict cases | Equal-generation conflict, foreign pilot, generation overflow, detected stale writer, unsupported intact slot, both corrupt and interrupted first creation each have explicit outcomes |
| PS-06 native semantics | Native fixture before/after buy, reload, next buy, oversized buy/sell and pending sales agree on exact integers/IDs; reproduce normalized port resources |
| PS-07 scope rejection | Non-stock loadout, remote second ship, nonempty mission/event/overlay or nondefault profile policy cannot be saved/imported as if supported |
| PS-08 application | Dock at Earth/Luna, save, exit/relaunch and resume same pilot/cargo/date; in-flight save rejected; R/trainer reset cannot overwrite a persistent campaign accidentally |
| PS-09 regression/budget | Existing combat/navigation modes retained; counted allocations and full peak guest memory stay within the measured envelope; failure paths bounded |

For PS-06 compare normalized native-parsed floating state with tolerances stated
before comparison; discrete gates, currency, quantities and identities remain
exact. DOS codec round trips require bit preservation; native text round trips
do not imply it. Native fixture and DOS fault-injection are different evidence.

## LIFE-06 staged compatibility route

1. **Now:** native source contract plus generated disposable oracle saves/traces.
   Keep each supported semantic field and normalization visible.
2. **After DOS codec:** host converter for a strict, declared fixture subset using
   the pinned native loader/writer. Input bundle includes needed pilot/global state;
   conversion reports consumed/unsupported content before writing a new DOS file.
3. **Qualification:** native → DOS → native (and reverse) comparisons of normalized
   state plus next action across supported fixtures. Never overwrite the source;
   fail unsupported outfits, missions, modified definitions and versions explicitly.
4. **Beta decision:** broaden the compatibility matrix only as actual campaign
   capabilities arrive. A full native campaign import is not safe while the DOS
   runtime cannot represent that campaign, regardless of parser acceptance.

Native plugin/settings availability matters more than filenames listed in a save.
Real user pilots remain outside current fixture work. General native interchange,
full campaign persistence and the future replay checkpoint remain open coverage
items until their implementations and gates exist.


## Implementation checkpoint — 2026-09-30

ESDOS-003 implements the bounded codec/store/navigation adapter. Envelope/state
validation, two-slot recovery, injected failures and DOS restart evidence are
indexed in CAMPAIGN.md. The epoch-zero scene still freezes calendar/world economy;
nonempty mission/event/overlay and general profile policies remain unsupported.
PS-06 currently covers native fixture state persistence/port normalization and
cargo-dependent movement; executing the production next trade awaits ESDOS-004.
PS-08 human restart acceptance is pending. PS-09 is the bounded scene's memory and
regression evidence, not a mature full-game budget certification. Future-envelope
recognition conservatively blocks writes before assuming a known CRC layout.
