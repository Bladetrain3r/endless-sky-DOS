# ESDOS work queue

Established 2026-09-30. Canonical local ticket record for this Git project until
an explicit migration. The [parity ledger](VANILLA-PARITY.md) tracks coverage;
[roadmap](ROADMAP.md) tracks sequencing. Neither implies that all future tasks
are ready for delegation. No BBS/Fossil mirror has been created.

State vocabulary: `proposed → ready → active → review → done`; `blocked` and
`awaiting_playtest` retain a next action/reason. A proposal is not authorization
for an agent to begin it. Status changes record date, owner, source/evidence and
remaining work. One active implementation per owner unless work is explicitly
split. Done requires acceptance evidence and review, not a delegate's assertion.

## Queue

| ID | State | Owner | Scope | Depends on |
|---|---|---|---|---|
| ESDOS-001 | done | astra_dawn; playtester Ziggy | Earth/Luna landing acceptance | none |
| ESDOS-002 | done | astra_dawn | Persistent-pilot and transaction contract, research/design only | none |
| ESDOS-003 | done | astra_dawn | Implement smallest durable pilot/save slice | ESDOS-002 |
| ESDOS-004 | proposed | unassigned | Two-port commodity trade loop | ESDOS-003; ESDOS-001 |
| ESDOS-005 | proposed | unassigned | Fuel/drive/system-transition slice specification | ESDOS-002 |

### ESDOS-001 — human navigation acceptance

- Created 2026-09-30; implementation local `44e4413e7`; no public release.
- Ledger NAV-04/PORT-01/PORT-02 **subsets only**.
- Run `python3 dos/flight/watch.py --navigation`. H then L lands on Earth and
  restores shields. Release/press L departs; P then L approaches Luna. W/A/D or L
  cancels approach before descent. Held L must not skip the dock screen.
- Already passed: source/profile-bound native/DOS gates and actual keyboard checks
  in [NAVIGATION.md](flight/NAVIGATION.md); earlier combat regressions pass.
- Acceptance: record approach/landing/departure feel, readable destination/dock
  information and control restoration. Record defects with destination/input order.
- 2026-09-30 human acceptance: user reports successful landing at both Luna and
  Earth in a quick playtest. Combined with existing automated gates, bounded
  landing checkpoint accepted. No claim of exhaustive manual edge-case testing.
- Closing this ticket accepts the bounded scene, not general campaign landing.

### ESDOS-002 — define persistent pilot and landed transactions

- Created/completed 2026-09-30; user-authorized design. Owner astra_dawn.
- Scope refined: assess LIFE-06 native interchange alongside the required DOS
  persistence contract, keeping compatibility claims separate from save/load.
- Deliver one reviewed design/acceptance contract under `dos/planning/`, with
  source anchors in PlayerInfo/Account/CargoHold/Ship/mission persistence.
- Scope: ownership and stable IDs; minimal ship/location/credits/cargo/date state;
  immutable content + mutable overlay boundary; landed transaction and save
  eligibility; one durable-write/recovery strategy suitable for DOS.
- Include a thin condition/event/mission identity example to avoid a trade-only
  model that cannot grow. No full interpreter, alternate database or UI rewrite.
- Specify DOS save format/version limits and how missing definitions are surfaced.
  Native save interchange remains a separate proposal, not assumed work here.
- Required acceptance design: native observable before/after state for a purchase;
  save/reload resumes the same state and next action; invalid/short/out-of-space
  write leaves the prior save recoverable; stale/deleted IDs never bind another ship.
- Output must name the fixture and result schema, test cases, numerical/integer
  invariants, file ownership and implementation boundary for ESDOS-003. Root reviews
  against upstream methods; no code delegation before that verification exists.
- Completed: [reviewed contract](planning/PERSISTENCE-CONTRACT.md),
  [source audit](planning/NATIVE-SAVE-AUDIT.md) and [executed native fixture](persistence/README.md).
  Six native checks pass: purchase/reload, next action, transaction isolation,
  quantity clamps, pending-sale reload and save eligibility. Root reviewed source
  and inspected trace/report. Design includes a mission/condition/event growth
  example; its nonempty state is deliberately unsupported in version 1.
- This design ticket specified two-slot DOS recovery and PS-01–09; subsequent
  bounded implementation/acceptance is ESDOS-003. No native-save compatibility claimed.

### ESDOS-003 — smallest durable pilot

- Created 2026-09-30; closed 2026-10-01 on human restart acceptance; owner astra_dawn.
  Implementation local `8828e304f`; no public release.
- Intended outcome: current dock/flight scene runs from persistent player-owned
  ship/location/credits/cargo state; landed save/restart retains it.
- Contract: [PERSISTENCE-CONTRACT.md](planning/PERSISTENCE-CONTRACT.md), PS-01–09.
  Ownership: new dos/persistence state/codec/store + tests and a small flight adapter.
  Implemented separate state/codec/store and dock adapter. Two bounded Sol delegates
  supplied codec/store + tests; root inspected code, corrected edge cases and ran
  combined native/DOS gates. No local-model inference or public deployment.
- Evidence: [CAMPAIGN.md](flight/CAMPAIGN.md), persistence-tests.json and
  persistence-application.json. Codec corruption,622 native/146 DOS short writes,
  injected stage failures, port/cargo normalization and actual process restarts pass.
- Human acceptance 2026-10-01: Ziggy flew Earth→Luna, pressed F5, exited and
  relaunched docked at Luna; returned to Earth, exited without a reported manual
  save, and relaunched at Earth. Manual save and landing autosave restart paths
  accepted. No additional human refusal/reset/fault-case testing claimed.
- Scope excludes full missions/fleets/native-save import. Existing trainers stay
  reproducible; trainer reset cannot overwrite a persistent campaign.
- Bounded pilot checkpoint closed; full LIFE-02–04 families remain partial.
  Native next-action trade gate transfers to ESDOS-004: persistence holds the
  native after-purchase state, but player-facing trade is not implemented here.

### ESDOS-004 — first actual port transaction

- Created 2026-09-30; proposed, owner unassigned. Depends on ESDOS-003 and 001.
- Intended outcome: buy/sell a native commodity at two ports with credits,
  capacity, cargo mass and price rules; repeated dock/trade/save/reload is coherent.
- Not ready: select stock fixture, quantity/price semantics and transaction owner
  after ESDOS-002; specify native clamping/no-op tests for insufficient funds/space/holdings.
- No inventing an economy to replace native behavior; frozen-fixture prices must
  be labelled until the full daily economy is integrated. Links PORT-03–05 subsets.

### ESDOS-005 — inter-system travel contract

- Created 2026-09-30; proposed, owner unassigned. ESDOS-002 completed; travel specification still to define.
- Intended output: bounded hyperdrive/fuel/active-system transition specification,
  native trace fixture and acceptance gates, compatible with future other drives
  and wormholes. Select one linked-system round trip, date/knowledge/fuel effects,
  and return/reload before broad routing or map UI. Links NAV-05/07/08/09 subsets.
- Not ready until player ownership contract exists; do not silently turn this
  into implementing all navigation types or a complete galaxy-map interface.

## Changes

- 2026-09-30: initial queue created after user requested ticketing and beta survey.
  Only ESDOS-001 describes already-implemented game work. The other tickets are
  prospective; no new gameplay or BBS runtime was started during the survey.

- 2026-09-30: ESDOS-001 closed on Earth/Luna human acceptance; ESDOS-002
  completed as design/native evidence; ESDOS-003 ready, no DOS save code yet.

- 2026-09-30: ESDOS-003 implementation/automated gates pass; human acceptance pending.

- 2026-10-01: ESDOS-003 closed after human manual-save and autosave restarts at
  Luna/Earth on8828e304f. Nightcap: stop development here; next is ESDOS-004 specification.
