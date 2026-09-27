# Ordinary bolts and the first firing range

The flight prototype now has a supplied training Energy Blaster. Hold Space to
fire at the stationary **PRACTICE TARGET** straight ahead of the reset position.
R resets flight, shots, hit count and spread seed. A HIT label flashes on contact;
the HUD counts shots, hits and live bolts. Background traffic and planets remain
cosmetic. The target is indestructible: no shields, damage, resources, friendly
fire rules, AI retaliation or ship-on-ship collisions are implemented here.

This is deliberately a training loadout. The original fitted Sparrow carries two
Beam Lasers; its flight parameters remain unchanged. This first gun uses a synthetic
centreline muzzle20 world units forward, not the ship's original hardpoints.
Original assets/data are untouched. Tracers are simple seven-pixel white lines,
not native projectile artwork, stretching, additive blending or impact effects.

## Reference contract

`native.py` links `oracle.cpp` against the unchanged original reference build's
objects and uses public `Projectile` constructor/`Move` APIs with the loaded
Energy Blaster. Twelve cases cover cardinal/wrapped headings, moving parents,
opposing motion and cancellation. It produces600 CSV rows: creation and49 calls
per case. Engine-reachable rows0–48 (588 rows) are compared with the portable C
kernel; row49 is retained only as a native post-removal diagnostic.

Acceptance set before comparison: heading/dead/removal flags exactly match;
position/velocity error <=1e-10. Native sanitizer comparison is exact; DOS maximum
state error is7.11e-15. This is sampled differential evidence, not a proof for all
weapons, states or inputs. DistanceTraveled/Clip are recorded by the oracle but
not ported/claimed by this kernel. No library-wide ISA audit is implied by build flags.

The loaded weapon supplies speed10.625world units/tick, lifetime48ticks,
reload12ticks (5shots/second at60Hz), inaccuracy3degrees. Zero acceleration,
drag, turning, homing, random speed/lifetime, fade and secondary emissions bound
this slice. Profiles are generated from hash-checked native metadata; physical
masks come from the earlier original-PNG collision oracle, never reduced artwork.

Native source findings retained in the integration:

- `Projectile` adds the parent's velocity to the shot's facing-dependent speed.
- `Move` predecrements lifetime. Calls1–47 advance position; call48 marks removal
  without another displacement.
- `Engine::CalculateUnpaused` moves existing shots, removes expired ones, adds
  newly fired shots, then resolves collisions. Newly fired shots therefore query
  their first segment without receiving a `Move` call on that tick.
- `CollisionSet::Line` queries position→position+velocity against target positions
  after ship movement. It does not subtract a target velocity from the segment.
- `Hardpoint::Fire` subtracts half the parent's velocity from the launch point;
  `BatchDrawList::Add` displays at position+half velocity. The trainer retains
  those offsets, but uses its explicit artificial muzzle.

`bolt_hit` finds the nearest eligible original-mask entry along the entire segment.
A hit at exactly fraction1 is a miss until the next interval, matching `Mask`.
Equal fractions retain the first supplied target; this is an explicit local tie
policy, not equivalence to Engine's faction/penetration/multi-damage ordering.
The demo has only one eligible target. Hit bolts disappear immediately rather
than retaining the native clipped projectile for its final rendering frame.

Triangular angular spread uses the same two-uniform-draw form as native default
`Distribution::GenerateInaccuracy`, with a small seeded LCG. Native RNG sequences
are **not** reproduced. The seed resets with R for repeatable practice.

## Boundaries, memory and checks

Sixteen fixed projectile slots prevent unbounded allocation; the current gun
needs at most four live slots in open space. Pool saturation drops a new shot,
counts it, and consumes the reload interval; it never overwrites a live shot.
There is no per-shot allocation. Training state is50,152B on DOS, predominantly
three fixed-capacity collision-mask buffers. This is not yet an all-ship cache.

`check.py` runs native ASan/UBSan (leak checking disabled), then the same checker
in DOSBox16MiB/fixed20,000cycles with CWSDPMI swap disabled. Besides the native
trace it covers whole-target tunnelling, nearest-hit order, equal-distance ties,
start-inside, miss, terminal boundary, lifetime expiry, 50target hits, misses and
expiry, release/reset and intentional pool exhaustion. The original mask library
has its separate11,772-query native equivalence record. The composition tests
are not an end-to-end native Engine oracle.

```sh
python3 dos/projectile/check.py                 # regenerate native oracle too
python3 dos/projectile/check.py --reuse-oracle  # hash-checked cached fixture
python3 dos/flight/run.py --practice-test --frames 300
python3 dos/flight/test_controls.py
python3 dos/flight/watch.py --pilot
```

The600-tick DOS app firing route produces50shots/50hits, no dropped shots. At
20,000cycles its frame cost is25.274ms average/29.985ms maximum, with2,897,866B
counted resident heap (excluding runtime/stack and480,000BVRAM). The `pilot_draw`
phase now includes the practice target, tracer drawing and hit label. These are
firing-range timings, not a complete-game combat budget. Actual DOSBox injected
Space plus flight/reset/camera keys also register shots and hits with no discarded
simulation time. Physical keyboard feel awaits human playtest.

Evidence: `../reports/projectile-equivalence.json`,
`../reports/flight-6-ships-practice-profile.json`, `../reports/flight-controls.json`.
Next: moving targets and native damage/shield/resource contracts before representing
this as ordinary combat; beams and real hardpoint/loadout export remain separate work.
