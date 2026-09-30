# Persistence implementation and native fixture

2026-09-30. The version-1 codec, two-slot DOS store and persistent navigation
pilot are implemented. Start with [CAMPAIGN.md](../flight/CAMPAIGN.md) for playtest,
ownership and evidence limits. The governing implementation contract is
[PERSISTENCE-CONTRACT.md](../planning/PERSISTENCE-CONTRACT.md); the source review is
[NATIVE-SAVE-AUDIT.md](../planning/NATIVE-SAVE-AUDIT.md).

From the project root, using the existing native reference image/build:

```sh
python3 dos/persistence/native.py
```

The runner links the fixture to unchanged upstream objects at
`061a9461a93898fb691504536589d1dcddc5d79b`. It rejects changed/untracked reference
source/data, runs without network in Docker, and uses a temporary configuration.
It never opens a user's pilot. Source/data hashes, image ID, compiler flags and
generated artifact hashes are recorded in
[native-persistence-contract.json](../reports/native-persistence-contract.json).
Full generated native saves remain under ignored `.work/persistence/`; the small
[native-trace.csv](native-trace.csv) is the durable observable-state record.

The scene uses a stock Sparrow, fixed ship UUID, default gamerules, Earth in Sol,
10,000 credits and Food at 505 credits/ton. It directly calls native trading,
serialization, load and landed initialization, without drawing UI. Initialize
`GameData::FinishLoading()` before creating/loading pilots: their reset path needs
the default registries it snapshots. Omitting it crashed the first harness;
upstream source required no modification.

## Executed checks

- Buy 3 tons: 8,485 credits, 1,515 cost basis, 3/15 tons held. Reload reproduces
  the selected normalized fields exactly, including stable ship identity.
- The next 1-ton buy gives the same result before and after reload: 7,980 credits,
  2,020 cost basis and 4 tons held.
- During `StartTransaction`, saving retains the prior serialized state despite
  live changes. `FinishTransaction` allows the new state to persist. No rollback
  of live memory is implied.
- An oversized purchase clamps to the remaining 12 tons of capacity; selling an
  oversized amount empties all 15 tons, clears cost basis and restores 10,000
  credits. Native pending sales become -15 and survive reload; buys add no entry.
- `CanBeSaved` accepts the landed pilot and rejects the in-flight and locked cases.

Compared fields are credits, commodity quantity/basis, capacity/free space, date,
unit price, pending sales, ship UUID, system/planet and normalized shield/hull/
energy/heat. All current comparisons are exact; no numerical tolerance is hidden.
The native reload restores port resources/IdleHeat, so this is not evidence of
preserving arbitrary in-flight resource values.

## Limits and next gate

This invokes the path serializer explicitly and tests save eligibility separately;
it does not exercise the public save method's backup rotation or global-file write
sequence. Reloads occur within one process using native `Load` then `Land`, not a
full executable restart. It does not compare every mission, profile or ship field,
and the random new-pilot identity/full campaign save bytes need not be reproducible
between runs. The checked trace and selected next action are the evidence.

The native oracle itself does not qualify DOS durability or native interchange.
Separate `python3 dos/persistence/test.py` gates cover the codec/store/adapter in
native sanitizers and reference DOSBox; `python3 dos/persistence/test_app.py`
exercises actual executable restarts and IRQ controls with disposable saves.
Human restart acceptance remains pending. Landed campaign saves and deterministic
in-flight checkpoints remain separate artifacts; broad campaign parity remains open.

## Implementation boundaries

- `state.h/.c`: bounded owners and semantic validation; typed content keys, IDs,
  calendar, stock scope, quantities/capacity and UTF-8 validation.
- `codec.h/.c`: field-wise little-endian TLV envelope, binary64, CRC32, staged decode.
  Recognizable future schema/header layouts are preserved as unsupported even if
  their checksum layout is unknown. An embedded NUL cannot hide trailing data.
- `store.h/.c`: alternating generations, stale/conflict checks, last-good recovery,
  exact write/flush/commit/close/readback checks and explicit failure injection.
  DJGPP libc fsync was inspected and found to tolerate some DOS errors; production
  DOS commits instead call INT21h/AH68h directly and reject carry/DPMI failure.
- `assets.py`: immutable build identity + native cargo profiles; no pilot mutation.
- `../flight/campaign*`: save eligibility, port normalization, input filtering and
  presentation. The existing trainers retain independent reset behavior.

`native-limits.json` now also exports stock capacity, bunk/crew/fuel limits and
all16 supported cargo-dependent motion/heat profiles from unchanged native getters.
These are included in oracle provenance and the development content identity.
The test-only after-purchase fixture retains the native 8,485 credits/3 tons/
1,515 basis. A player-facing trade operation and next-purchase differential gate
remain ESDOS-004; serializing these fields is not a claim of trading gameplay.
