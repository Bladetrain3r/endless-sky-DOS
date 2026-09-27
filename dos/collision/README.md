# Collision qualification

This slice exports native `Mask::Create` outlines from original full-alpha ship
images and compares a bounded portable C query implementation against original
`Mask::Contains` and `Mask::Collide`. It does not regenerate hitboxes from reduced
or quantized artwork. Starting inside returns 0; no hit returns 1; other values
are first-entry fractions along a displacement segment. Polygon winding,
half-open edges and the circular early rejection retain native rules.

Acceptance before running: identical containment and hit/miss decisions, with
collision fraction error at most 1e-10. Exact boundary cases are included; numerical
disagreements must be reported, not silently removed or waved through because
their coordinate errors are small. Source/original assets stay unchanged.

## Scale correction discovered here

`source/image/Mask.cpp:SmoothAndCenter` produces outlines at half the PNG's pixel
dimensions. `source/Body.cpp:Body::Unit` also multiplies by 0.5 before rendering.
Checking Sprite::Width alone missed that second stage in the earlier renderer
research. The current DOS prototype draws full PNG sizes while positions use
native world units. Its flight arithmetic is qualified, but its displayed hulls
are therefore twice native logical size at a one-pixel-per-world-unit view.

Before integrating shooting, correct the world-to-screen mapping (and verify
sprite/mask alignment at rotations). Keep physical masks in native world units;
do not enlarge hitboxes to fit the old prototype's art. The accepted pilot build
is retained while this separate foundation is being qualified.


## Result and reproduction

Run `python3 dos/collision/check.py` from any directory. It rebuilds the native
oracle in Docker, builds the portable checker under native ASan/UBSan, then builds
and runs the same checker in DOSBox at 16 MiB / fixed 20,000 cycles with swap disabled.
`--reuse-oracle` uses hash-checked local reference outputs. Eight malformed mask
files are rejected under sanitizers. Leak checking is disabled.

All 11,772 queries pass for original Sparrow, Star Barge and Falcon geometry:
identical inside/outside and hit/miss decisions, including vertices, edge
midpoints, zero-length segments and rotated boundaries. Native fractions match
exactly; DOS maximum fraction error is 4.44e-16. Evidence and original image/source
hashes: `../reports/collision-equivalence.json`. This is a sampled differential
qualification, not an exhaustive proof for every possible line and orientation.

An initial text-transfer version had 212 DOS disagreements at exact boundaries.
Switching geometry and query transfer to binary64 removed them without changing
collision tolerances. Both default DOS x87 precision (control bits 768) and explicit
53-bit precision (bits 512) pass the binary fixtures. A global FPU change is not
required by this evidence. Exported text is retained for inspection; DOS must
consume the packed doubles, not round-trip precise geometry through its decimal
parser. Movement's looser numeric tolerances had not exposed this boundary issue.

The little-endian `ESMASK1\0` binary starts with a u32 mask count; each mask has
u32 ID/outline-count/point-count and binary64 radius, followed by each outline's
u32 length and binary64(x,y) vertices. Bounded runtime storage is 49,392 static bytes
for these three masks (deliberately fixed capacity), while the packed geometry
file is 2,060 bytes. The shapes contain 123 points in five outlines total. This is
not yet a production all-ships cache or collision broad-phase benchmark.

Current flight gameplay/executable is unchanged. Before weapons integration:

1. Correct the body/image world-scale contract and check rendered masks visually.
2. Compare a bounded ordinary projectile's lifetime, inherited velocity and
   swept-segment ordering against native Projectile/Engine behavior.
3. Add target selection and a practice-fire loop, with resource/damage omissions
   explicit until those systems are ported. The trainer idea is in `../WISHLIST.md`.
