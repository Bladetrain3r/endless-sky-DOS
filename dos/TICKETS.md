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
| ESDOS-001 | awaiting_playtest | astra_dawn; playtester Ziggy | Earth/Luna landing acceptance | none |
| ESDOS-002 | ready | astra_dawn | Persistent-pilot and transaction contract, research/design only | none |
| ESDOS-003 | proposed | unassigned | Implement smallest durable pilot/save slice | ESDOS-002 |
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
- Next action: await the user's planned playtest; no repeated prompt required.
- Closing this ticket accepts the bounded scene, not general campaign landing.

### ESDOS-002 — define persistent pilot and landed transactions

- Created 2026-09-30; ready to pick up, **not started**. Owner astra_dawn.
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
- No dependency on human flight feel or BBS availability. This is the recommended
  next engineering task; this ticket alone does not launch background work.

### ESDOS-003 — smallest durable pilot

- Created 2026-09-30; proposed, owner unassigned. Depends on ESDOS-002.
- Intended outcome: current dock/flight scene runs from persistent player-owned
  ship/location/credits/cargo state; landed save/restart retains it.
- Not ready: exact ownership/format, native fixture and interruption gate must be
  supplied by ESDOS-002 before implementation assignment. Scope excludes full
  missions/fleets/native-save import. Existing trainers stay reproducible.
- Closure needs the specified native/DOS roundtrip/recovery gates and human restart
  check, source revision and evidence links. Links LIFE-02–04 subsets.

### ESDOS-004 — first actual port transaction

- Created 2026-09-30; proposed, owner unassigned. Depends on ESDOS-003 and 001.
- Intended outcome: buy/sell a native commodity at two ports with credits,
  capacity, cargo mass and price rules; repeated dock/trade/save/reload is coherent.
- Not ready: select stock fixture, quantity/price semantics and transaction owner
  after ESDOS-002; specify rejection tests for insufficient funds/space/holdings.
- No inventing an economy to replace native behavior; frozen-fixture prices must
  be labelled until the full daily economy is integrated. Links PORT-03–05 subsets.

### ESDOS-005 — inter-system travel contract

- Created 2026-09-30; proposed, owner unassigned. Depends on ESDOS-002.
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
