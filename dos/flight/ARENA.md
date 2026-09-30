# Two-opponent combat arena — 2026-09-30

Launch the staged build from any directory:

```sh
python3 /data/NuCode/Codex/endless-sky-DOS/dos/flight/watch.py --arena
```

A Sparrow and two pursuing Star Barges share an open training area. Six harmless
navigation buoys mark a ring 900 world units from the starting point; they are
visual reference points, not obstacles or a boundary. Both enemies start within
view and begin firing after two seconds. There are no waves or respawns until
reset. The player retains stock health; the enemies default to training health.

| Control | Action |
|---|---|
| W / Up | Thrust |
| A / D or Left / Right | Turn |
| Space | Fire along the ship's heading |
| T | Cycle living targets, including disabled hulls |
| N | Select the nearest living target |
| Tab | Switch following / fixed camera |
| R | Restore all three ships, resources, shots, selection and grace timer |
| H | Drain 25% of player shields once per press; trainer input |
| Escape / Ctrl+F9 | Exit normally / quit DOSBox |

The reference window remains 800×600, silent, with a 120-second session limit.
There are no reverse engines. Existing `--opponent-stock`, `--opponent-stress
energy|heat`, `--resource-stress energy|heat` and `--invulnerable-target` options
also work with `--arena`. These are trainer overrides, not new game loadouts.
The single-opponent `--pilot --pursuit` and quiet `--pilot` modes remain available.

## First cockpit layout

The lower panel groups the player's shield, hull, energy and heat; selected
ship status, health and range; and a small player-centred, north-up radar.
Yellow brackets or an off-screen diamond identify the selected ship. Radar
contacts outside its range clamp to the edge with a white centre. The top bar
shows camera mode, speed in world units/second, health preset and enemy count.

Selection is informational: **it does not aim the gun or restrict collisions**.
A shot hits the first enemy hull it intersects, even if another ship is selected.
A destroyed selection changes to the nearest survivor. No survivor means no
target. Disabled ships count as living until hull falls below zero. Victory
means both enemy hulls are destroyed; shots already in flight remain dangerous.
This is a cockpit foundation, not a final UI or a complete native interface.

## Ownership and ordering

`arena.c` owns two fixed enemy slots, each with motion, health, shield recharge,
energy/heat, firing cooldown, statistics and a 16-shot pool. Immutable masks,
art and arithmetic kernels are shared. The player still owns one 16-shot pool
in `Practice`; `Threat` supplies the single player hull and damage handler.
Its legacy hostile pool is unused in arena mode. `arena_view.c` only reads state;
it neither allocates nor consumes firing randomness.

Per tick: player recharge/generation/movement; both enemy recharge/generation/
movement; player shot movement/fire and collision against all living enemies;
each hostile pool's movement/fire and collision against the player. A friendly
hit that disables or destroys an enemy suppresses its same-tick new shot.
Existing shots survive their owner's death. Disabled enemies retain native
momentum decay; destroyed ships freeze/hide. After player death, enemies stop
receiving pursuit updates. R starts the whole encounter again.

The two enemies always regard the player as hostile. They use native-qualified
Barge movement/resource helpers and predictive interception with the same
artificial blaster turret as the earlier trainer. No general faction relations,
enemy-friendly-fire, ship collisions, obstacles, loot, missions, landing or
complete native AI behavior are claimed. The loader still stages Sol and the
original six assets; arena mode replaces their scene presentation. Removing
unused scene allocations is deferred until the runtime scene contract expands.

## Evidence and next playtest

All builds/tests use Docker. DJGPP targets i386 without MMX/SSE, DOSBox uses
16 MiB, 20,000 fixed cycles and no swap; cycles are not calibrated hardware MHz.

- `test_arena.py`: native ASan/UBSan and DOS agree on independent resource gates,
  nearest collision, same-tick disable/kill, owner-death shots, shared player
  damage, selection/reset and fixed pool exhaustion without charging a shot.
- `test_arena_view.py`: 88 cases with actual sprites and the production font;
  native ASan/UBSan including leaks, extreme positions, clipping, camera offsets,
  death/selection states, deterministic redraw and read-only simulation state.
- `run.py --arena-test --frames 450`: 900 player movement ticks still match the
  native route within 1e-7 position/velocity and exactly in heading. Both enemies
  fire; no pool drops. This route makes targets invulnerable and does not fire.
- `test_controls.py --arena`: real DOSBox key injection, held T cycles once,
  N is delivered, reset restores the encounter, camera toggles, both enemies
  fire at the same player, and no simulation time is discarded.
- Existing power, threat, opponent and IRQ tests pass; quiet range and pursuit
  application keyboard regressions also pass.

Reports: `dos/reports/flight-arena-*.json`, `flight-controls-arena.json` and
`flight-6-ships-arena-profile.json`. Root inspected the DOS framebuffer capture.
The arena route counts 2,901,342 heap bytes, 2,100 more than the previous trainer;
code, stack, allocator/runtime and VRAM are excluded. Mean uncapped frame time
was 29.361 ms, max 34.344 ms, on this bounded route. That is not a whole-game FPS
claim. Arena rendering is measured as one draw pass; legacy per-layer timing
fields are unmeasured zeros in arena mode.

Human acceptance pending: fight both ships, cycle targets while firing, follow a
contact off-screen, and reset after a kill. Check HUD readability and whether
the lower panel leaves enough useful flight space. Next scope should follow
that feedback: navigation/landing or broader combat behavior, rather than adding
more status text speculatively.
