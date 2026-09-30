# Persistent development pilot

2026-09-30 — ESDOS-003 implementation checkpoint. This is the first saved pilot
inside the bounded Sol navigation scene, not a complete campaign or native-save
importer. [Save contract](../planning/PERSISTENCE-CONTRACT.md),
[implementation/evidence](../persistence/README.md), [queue](../TICKETS.md).

## Playtest

```sh
python3 dos/flight/watch.py --campaign
```

The first launch creates **First Pilot**, with a stock Sparrow named **First
Light**, 10,000 credits and an empty 15-ton hold, docked at Earth. These are
explicit development starting conditions, not a vanilla start/conversation.
Subsequent launches resume the saved pilot at the last saved port.

- **L** departs, or approaches the selected destination in flight.
- **P** selects Earth/Luna. W/A/D, camera and approach cancellation remain as before.
- **F5** saves while docked; holding it saves once. In flight it displays a reminder
  to land and leaves the save untouched.
- Landing saves automatically; departure saves before launch. A failed departure
  save keeps the ship docked, including subsequent polls of a held L key.
- **Escape** exits. In-flight position is not a campaign save; restarting resumes
  at the last saved dock. The existing 120-second launcher session limit applies.
- **R** reports that reset belongs to the trainers and keeps the pilot. H and the
  trainer blaster are inactive here. Combat trainers remain available separately.

Suggested acceptance: launch, depart Earth, select/land at Luna, check the save
message, exit and relaunch. You should be docked at Luna with the same pilot and
credits. Try F5 in flight, and R at the dock. No new trade UI exists yet.

## State and data

`Campaign` owns pilot/ship identity, date, credits, pooled cargo/basis, pending
sales and the acknowledged disk generation. Existing navigation/flight structs
own live movement and transient resources; eligible save boundaries snapshot
resources/location into the campaign. Loading normalizes the landed port state
before presentation, clearing projectiles/input and restoring services/IdleHeat.

Native-exported cargo profiles cover 0–15 tons, including acceleration, turning,
drag and heat capacity. A restored 3-ton fixture therefore carries 3 tons in
flight as well as in the ledger. Fuel/crew are represented and normalized at these
two supported ports; fuel consumption, daily advancement/economy, missions,
outfitting, arbitrary fleets and other systems are not implemented. The date and
Sol layout remain frozen. Stock beam weapons are not implemented, so this pilot
mode cannot fire the substitute trainer gun.

Files live in `.work/flight/run/SAVE/`: `SEED.DAT` supplies two host-generated UUIDs
only for initial creation, while `PILOT0.SAV`/`PILOT1.SAV` alternate generations.
The launcher never recreates a pilot over damaged/unsupported save files. The
build leaves this directory alone. Copies should retain both slots. No native
pilot directory or user-owned save is accessed by these tools.

`SAVEKEY.json` hashes the immutable world pack, supported runtime profiles,
native capacity/cargo limits and relevant gameplay kernels. Its SHA-256 is
compiled into `SAVEKEY.H`; `BUILD.json` binds that header/manifest to the
executable. The launcher checks this binding and takes an exclusive lock for its
shared run directory. Changed content is rejected rather than silently importing
old state. This is a development format, not a compatibility guarantee through
future builds. The portable DOS store itself is single-writer; generation checks
are stale-session detection, not an interprocess lock.

## Evidence and limits

- `../reports/persistence-tests.json`: native ASan/UBSan/leak checks and DJGPP/
  DOSBox codec, store and adapter gates. Native exhausts 622 short-write cases;
  DOS samples146 (all header bytes, regular payload offsets and final byte).
  Both cover all injected stage failures. Adapter covers seeded nonempty cargo,
  port reload/normalization, native cargo-dependent motion, held F5/reset,
  blocked departure, in-flight save rejection and failed landing-autosave dirtiness.
- `../reports/persistence-application.json`: actual executable/IRQ and separate
  process restarts, Earth→Luna, saved identity/credits/date, corruption recovery
  and refusal to overwrite a recognizable future format.
- Navigation900-frame and arena450-frame regression reports, plus IRQ decoder and
  vector-restoration checks remain the regression evidence.

The store bounds extra temporary heap at 262,404 bytes; this is reported separately
from resident allocations. Saves perform DOS INT21h/AH68h commit with carry/error
checking, then close and read back/decode the candidate. Tests cover emulated
process/I/O failures, not a physical power cut or real FAT/controller guarantees.
A late failure can leave a valid newer candidate discoverable on restart even
though the failed operation reported no success. Recovery/unsupported errors are
visible; no automatic migration or destructive repair is attempted.
