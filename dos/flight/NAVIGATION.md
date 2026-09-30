# Sol landing loop — 2026-09-30

```sh
python3 /data/NuCode/Codex/endless-sky-DOS/dos/flight/watch.py --navigation
```

This quiet scene has Earth and Luna, destination selection, physical autopilot
approach, landing, a dock screen and takeoff. The accepted two-opponent arena
remains available with `--arena`. The reference window is still 800×600, silent,
and bounded to 120 seconds. Navigation starts in the stock Sparrow near Earth.

| Control | Action |
|---|---|
| P | Cycle Earth / Luna; cancels an active approach |
| L in flight | Begin or cancel the selected destination's landing approach |
| L on the dock screen | Take off |
| W / Up, A / D or arrows | Manual thrust/turn; cancels approach before descent |
| Space | Fire the training blaster into empty space |
| Tab | Following / fixed camera |
| R | Reset ship and navigation encounter |
| H | Trainer shield drain, once per press, while in flight |
| Escape / Ctrl+F9 | Exit normally / quit DOSBox |

For a short test, **H, then L** approaches Earth and lands. Shields should be
restored at the port. Release and press **L** to depart, then **P, L** flies to
Luna and lands there. Manual steering or another L press cancels an approach.
Once descent starts, let it finish; controls resume after the takeoff animation.
Controls held during departure must be released before they can move/fire again.

The cockpit shows the destination, distance, native landing radius and whether
you are too far away, too fast, approaching or ready to land. The dock screen
shows the original planet description, native facilities and current ship state.
Trading, outfitting, ship purchases and missions are explicitly unavailable;
this screen supplies the state transition on which those systems can be built.

## Supported native behavior

The approach helper follows `AI::MoveToPlanet` / `MoveTo` / `StoppingPoint` for a
fully crewed stock Sparrow: ordinary forward thrust, no reverse engine,
afterburner, hyperspace or cruise-speed override. It accounts for turning and
braking distance; it does not teleport or snap the velocity to zero. Landing
requires speed **strictly below 1 world unit/tick** (60 units/second), distance
**strictly inside** the stellar radius, permission, and an operational ship.

The first landing-command tick still coasts. Following ticks draw the ship 3%
of the remaining gap toward the planet centre and reduce binary32 zoom by the
native `.02f` default; the existing generation/recharge order continues. The
last fade tick enters a frozen dock state. A newly disabled/overheated ship
interrupts descent. Rendering uses a small nearest-sample scale routine for
this transition; the original smooth shader/landing-camera zoom is not ported.

Both ports allow landing and services under the snapshot's native initial
reputations (`Politics::Reset`). Docking restores the supported shield, hull
and energy budgets, with heat set to native Sparrow `IdleHeat`, about 3,766.
A warm heat bar after docking is therefore expected. No fuel, cargo, crew,
calendar, mission, repair-cost or saved-game state exists in this slice.

Takeoff uses the native angle/radial distribution within the planet radius and
one world unit/tick outward velocity. A separate seeded trainer LCG chooses the
position/heading, so repeated resets are reproducible; this is not the native
campaign RNG stream. The ship grows back to full size before control returns.
No firing or steering is applied during descent/takeoff. Shots are cleared when
entering the dock screen. Holding L across docking cannot initiate departure.

## Data and ownership

`navigation.c` owns destination/state/selection and the quiet player's update
order. It reuses the existing resource, propulsion and projectile kernels.
`navigation_view.c` reads this state without allocations or simulation changes.
The main loop pauses space ticks on the dock screen and reuses its static frame;
wall-clock session timeout continues. It does not accumulate missed world time.

`dos/navigation/` contains the bounded approach kernel and its native oracle.
The oracle exports exact Sparrow movement parameters as `APPROACH.DAT` and a
resolved Earth/Luna permissions, service and description snapshot. The runtime
loads a 2,160-byte `NAV.DAT`; no planet service/permission is inferred merely from
a sprite name. `navigation_assets.py` gets landing radii from source PNG size:
`StellarObject::Radius` is half the native half-PNG dimensions, giving Earth45
and Luna20. Padded runtime sprite canvases do not determine landing geometry.

Positions retain the existing epoch-zero Sol Earth/Luna geometry, translated
near the trainer origin. Only these two destinations are instantiated; this
is not a system-wide selection implementation or dynamic campaign politics.
Both metadata and profile loads validate their bounded formats before replacing
live state. Descriptions and converted sprites retain upstream notices/terms.

## Verification

All builds/tests use Docker; DJGPP i386/noMMX/SSE, DOSBox16MiB/20k cycles, no swap.
Cycles are not MHz. `dos/reports/` records source/profile hashes and results.

- `python3 dos/navigation/check.py`: 20 native AI command cases compared with
  C under ASan/UBSan and DOS; exact flags, zero observed turn error. Root reran
  and reviewed the delegate's helper/oracle. This qualifies the approach helper,
  not the full campaign landing/takeoff implementation.
- `python3 dos/flight/test_navigation.py`: native ASan/UBSan and DOS strict
  range/speed/state/permission gates, cancellation, two physical approaches,
  port restoration, frozen dock, held-key safety, takeoff geometry, control
  release gate, reset, interrupted descent and transactional malformed load.
- `python3 dos/flight/test_navigation_view.py`: 88 actual sprite/font cases under
  ASan/UBSan/leak checks; scale/edge/far coordinates, dock text, state/camera
  variants, deterministic redraw and no mutations to simulation state.
- `python3 dos/flight/run.py --navigation-test --frames 900`: Earth landing,
  paused dock, takeoff, Luna selection/approach/landing and takeoff, using the
  regular simulation. Two landings/takeoffs; no teleport, enemy shots or swap.
- `python3 dos/flight/test_controls.py --navigation` and `--navigation --launch`:
  actual DOSBox keyboard checks, including holding L through docking, port
  restoration, departure, restored fire control and approach cancellation.
- IRQ latch tests and arena/quiet/pursuit application keyboard regressions.

Counted navigation heap is 2,901,550 bytes, excluding code/stack/runtime/VRAM.
The dock's initial full text draw is slower than ordinary flight; subsequent
frames reuse it. Whole-route averages mix flight and dock frames and are not a
whole-game FPS claim. Per-layer legacy timing fields are unmeasured zeros here.
Root reviewed the captured dock and flight views; human acceptance is pending.
