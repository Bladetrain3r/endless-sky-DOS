# Roadmap to feature-parity beta

2026-09-30. **Accepted destination:** vanilla parity at beta; original additions
through ESDOS 1.0 afterward. **Proposed ordering below:** engineering recommendation,
not a schedule or evidence of implementation. Baseline and complete feature ledger:
[VANILLA-PARITY.md](VANILLA-PARITY.md). Concrete queue: [TICKETS.md](TICKETS.md).

## Present checkpoint

Flight, resources, ordinary projectiles/hits, two-opponent arena and cockpit have
bounded native/DOS evidence and human playtests. The Earth/Luna landing slice is
implemented and automatically checked, awaiting human acceptance. We have a
useful sandbox, not a campaign runtime. No full-game completion percentage yet.

## Sequence

| Stage | Deliverable | Exit evidence and ledger coverage |
|---|---|---|
| 0: preserve the baseline | Human landing/takeoff feedback; compact reproducible scene | Existing arena remains accepted; navigation documented; NAV-04 subset |
| 1: a pilot who persists | Explicit mutable pilot model, durable stable IDs, credits/cargo/date/location/ship; safe landed saves | Restart round trips and interrupted-save recovery; LIFE-01–04 and foundations for PORT/STORY |
| 2: first civilian loop | Buy cargo, fly/land elsewhere, sell, buy an upgrade; correct credits/mass/capacity and service checks | Native transaction fixtures, no duplication/loss across reload; initial PORT-01–06/09/10 |
| 3: connected universe | Fuel, drives, system transition, map knowledge/routes and bounded active-world residency | Travel/refuel/return/reload through different systems, no unknown-map leaks; NAV-05–11, UI-02 |
| 4: general encounter and fleet | Real loadouts/weapons, factions/AI, mining/loot/boarding, owned escorts and carriers | Mixed battle/harvest/rescue/capture/carrier traces; FIGHT and FLEET families |
| 5: campaign engine and services | Conditions/conversations/actions/events, jobs/story NPCs, economy/accounts/crew, complete shops/storage | Delayed event → changed world/shop → reload exactly once; full STORY/PORT integration |
| 6: content-complete play | All pinned stock definitions; starts/campaign families, all menus/logs/settings, persistence and audio | Schema/route coverage, major story completion and branch/failure cases; all ledger families |
| 7: beta qualification | Package, resource/performance gates, benchmarks, long-session tests and compatibility record | Full beta exit checklist; same candidate for correctness, performance and human playtest |
| After beta → 1.0 | Our own additions and polish | Separate proposals; do not relabel unfinished parity as optional flair |

Stages are not isolated waterfall silos. A thin mission/condition/event fixture
should enter while designing persistence, before stage 5, so we do not bake a
save model incapable of storing a campaign. Start audio/toolchain feasibility and
full-content memory measurements early too. Carry a repeatable trade/travel/mission
scenario forward as a regression spine; grow it rather than restarting each demo.

Avoid spending months on a beautiful outfitter before a bought outfit survives
reload, or profiling a three-ship fight as if it certifies a large campaign fleet.
Conversely, do not delay small useful playable slices until all 2,344 raw mission
declarations have an interpreter. Implement the common contracts once, test their
observable behavior, then widen content coverage.

## Architecture pressure points to settle early

1. **Immutable content versus mutable state.** Retain the indexed pack for base
   definitions, with an explicit mutable event/universe overlay. No database rewrite
   is assumed. Benchmark real access before changing the existing storage decision.
2. **Identity and persistence.** Stable identifiers for pilots, ships, missions and
   content; clear owners of cargo/economy/conditions. Do not persist raw pointer or
   positional-pack IDs. Atomicity/recovery is a design contract, not an afterthought.
3. **General actors.** Reuse qualified kernels, replace trainer-specific assumptions
   deliberately. Specify overflow behavior for bounded DOS pools; silently dropping
   a consequential actor or projectile is not a successful optimization.
4. **Rules and presentation.** Keep simulation at its reference timebase and draw
   at an affordable rate. Reduced effects are distinct from reduced simulation.
5. **Evidence scope.** A math oracle, integrated scenario, full-content validation,
   long play session and memory measurement answer different questions.

## Decisions without an immediate implementation blocker

- Freeze the already-qualified upstream revision for beta; refresh via explicit
  delta work later. This is the working assumption, not a claim it is latest.
- Preserve vanilla gameplay/content, while using functional DOS presentation.
- Native save interchange and unrestricted third-party plugins need compatibility
  policies; neither uncertainty prevents implementing our own correct persistence.
- Audio conversion/backend and performance envelope require measured candidates.
  No silent approval of crowd/economy reductions or a worse timebase.
- BBS adoption is a parallel tooling proposal, not a prerequisite to game work.
  See [the workboard assessment](../../docs/BBS-TASK-BOARD-ASSESSMENT.md).
