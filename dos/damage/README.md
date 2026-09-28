# Ordinary shield and hull damage

This slice compares the original `DamageProfile::CalculateDamage` and
`Entity::TakeDamage` against a small C kernel. The native oracle links unchanged
reference objects and uses a synthetic, unprotected Entity with the loaded
Energy Blaster. It exercises the actual native resource calculation/application,
not a second handwritten damage formula. The fixture deliberately omits Ship's
crew, disabled-state events, repair, regeneration, government and AI behaviour.

For this supported slice, shields block hull damage. If the shot would deal more
shield damage than the shields remaining, the absorbed fraction is
`shields / shield_damage`; the remainder of the shot applies its hull damage.
Shields clamp to zero, while hull can become negative. Native destruction is
strictly **hull < 0**; exact zero is not destroyed. The Energy Blaster defaults
its disabled damage to its normal hull damage, so that threshold does not change
this weapon's ordinary resource loss. This does not port disabled-ship behaviour.

Eight cases / eighteen hit rows cover full and partial shields, exact depletion,
bare hull, exact zero hull, already-zero hull, and repeated hits through death.
The exact-boundary fixtures use the native weapon getter values: the original
parser yields10.600000000000001 shield and6.6000000000000005 hull damage.
`assets.py` exports those values to `DAMAGE.DAT`. Both host and DOS checkers use
its text parser, including exact-zero cases; the observed transfer passes this
bounded fixture. Do not generalize this to arbitrary decimal/boundary transfer:
the collision mask work required binary64 geometry.

Acceptance: absolute resource/dealt error <=1e-10, exact destroyed flags.
Native portable ASan/UBSan max error1.78e-15; DOS max3.55e-15. Sanitizers cover
the portable checker/kernel, not the original prebuilt native objects; leak
checking is disabled. DOS:16MiB/fixed20,000cycles/i386/noMMX/noSSE/CWSDPMI-s-.
Original source, data and assets remain unchanged.

```sh
python3 dos/damage/check.py                 # regenerates native reference rows
python3 dos/damage/check.py --reuse-oracle  # validates cached reference hashes
```

Evidence: `../reports/damage-equivalence.json`. The profile and behavior-source
hashes are checked before staging. Unsupported: piercing, permeability,
disruption, cloaking, protection, relative damage, healing, blast scaling,
status damage, resource costs, regeneration and full Engine damage ordering.

## Destructible training target

The default pilot scene now uses deliberately reduced **30 shields / 26 hull**,
not stock Star Barge health. Seven connected blaster hits destroy it. The lower
HUD shows shields/hull (displayed hull clamps at zero; internal state retains the
negative native result). Hits after destruction do not count; target motion and
collision stop at the death position. Remaining and newly fired bolts continue
normally. R restores health, route, counters, projectiles and firing seed.

A48-tick procedural spark burst marks destruction. It is cosmetic: no native
explosion artwork, damage radius, debris, salvage or sound. Its drawing consumes
no firing RNG, allocates no particles and clips to the world viewport. Tests
cover all48 effect ages at screen edges, unchanged RNG and buffer guards, reset
during the explosion, no subsequent target hits, and frozen moving-target state.
The old range is available with `watch.py --pilot --invulnerable-target`; add
`--stationary-target` for the fixed version. The player gun now shares the propulsion energy/heat budget. The optional
incoming-fire trainer uses an explicitly unlimited-power artificial enemy turret;
see `../flight/README.md` for player shield, hull and disable behavior.

```sh
python3 dos/flight/watch.py --pilot
python3 dos/flight/run.py --practice-test --destructible-target --frames 300
python3 dos/flight/run.py --practice-test --moving-target --destructible-target --frames 480
python3 dos/projectile/check.py --reuse-oracle
python3 dos/flight/test_controls.py
```

The stationary600-tick app route records50shots,7hits,0drops; destruction at tick84,
with no subsequent hits. Frame cost25.932ms average/30.762ms maximum, counted heap
2,897,942B excluding runtime/stack and480,000BVRAM. Actual injected-key firing
also destroys the target in7hits, with no discarded simulation time. Root inspected
`.work/flight/explosion-early.png`; Sept28 human playtest confirms the seven-hit kill, visible explosion and R reset.
This is a firing-range budget, not a full battle benchmark.

The rotating moving-target app route also passes:80shots,7hits,death at tick949,
26.163ms average/30.669ms maximum over960ticks. See the matching
`flight-6-ships-practice-moving-damage-profile.json` report.
