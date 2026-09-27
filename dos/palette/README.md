# Early indexed-color contract and proof

Provisional decision (2026-09-27): **one shared 256-entry flight palette**, with
16 fixed UI entries, 32 grayscale entries, and 208 learned art entries. Train
on a versioned, balanced art sample, never independently per sprite. The initial
six-asset training set is intentionally small; three held-out assets expose
some generalization error. Do not freeze this palette for the whole game yet.

Use six-bit channel values in the palette file as the conservative hardware
contract. Its 768 bytes are DAC values, not eight-bit RGB. The proof expands
these values back to RGB for inspection. This is not yet a hardware palette
programming test. Later full-screen landing/menu art can use another palette
at a scene boundary, retaining UI entries; never switch for individual ships.

## Source evidence and boundaries

Upstream `source/image/ImageBuffer.cpp` stores 32-bit color and premultiplies
alpha (see `Premultiply`). `ImageFileData.cpp` and `BlendingMode.h` distinguish
normal, premultiplied, additive and half-additive assets through naming.
`shaders/sprite.frag` blends animation frames and supports masked color swizzles;
`SpriteShader.cpp` supplies masks and matrices. A generic RGB conversion would
lose behavior. The proof rejects explicit special blend modes and swizzle masks;
it only exercises normal-alpha still images, PNG/JPEG, from a fixed manifest.

Proposed runtime asset representation: indexed color plus separate coverage.
The experiment packs two four-bit alpha values per byte, low nibble first.
Opaque black is a real color, not a transparency key. Four-bit coverage is an
experiment, not an accepted visual equivalence tolerance. Fully opaque assets
can omit coverage later. Runtime transparency might use a bounded blend table
or stippling; neither performance nor fidelity is qualified by this script.
A full 16-level 256-by-256 lookup alone would take 1 MiB, so do not casually
allocate one per effect. Recolor before quantization, or qualify palette remaps
against the source masks/matrices; arbitrary swizzles cannot simply be dropped.

Original logical dimensions, animation cadence and collision masks must remain
independent of image reduction (`source/image/ImageSet.cpp`). The 192x144 cap in
this contact sheet is an experiment only, not a proposed universal ship scale.
The current sample says nothing about animation shimmer, effect overlap, or
runtime rotation. These must join the next representative flight scene gate.

## Reproduce and inspect

From the checkout root, using the already available tools image:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -e HOME=/tmp -v "$PWD:/work" -w /work modern-arena-tools:0.1 \
  python3 dos/palette/proof.py
```

Pillow9.4.0 was used. Outputs under ignored `.work/palette/`:

- `comparison.png`: true-color resized reference on the left of each pair;
  indexed-color/four-bit-alpha composition on the right. T means training,
  H held out. The reference columns are deliberately **not** palette-reduced.
- `FLIGHT.PAL`: 256 RGB triples, each channel0–63.
- `A000.IDX` etc: raw row-major indices, plus separate `.A4` coverage.
- `report.json`: input hashes, sizes, palette hash, format inventory and errors.

The persisted `dos/reports/palette-baseline.json` records this first run. Error
is mean absolute RGB-channel difference after compositing over a single dark
background, before contact-sheet thumbnailing. It excludes resize loss and is
not a perceptual acceptance threshold. No spatial dithering is used; avoid
committing to moving-sprite noise without animation inspection.

At equal dimensions, index+four-bit alpha is 1.5 bytes/pixel versus RGBA4:
62.5% less uncompressed pixel storage, before headers, masks and lookup tables.
An800x600 indexed framebuffer is480,000 bytes; a second buffer doubles that.
None of these numbers establishes whole-game RAM or render speed.

Root inspected the contact sheet: ships remain recognizable; muted shadow
ramps and landscape gradients need a larger scene test. The held-out lava world
is visibly less faithful than the training set. Keep the experiment, expand its
coverage before selecting final art colors. No upstream assets were modified.
Converted assets retain original terms: consult repository `copyright`; do not
redistribute this output without the applicable per-asset notices.
