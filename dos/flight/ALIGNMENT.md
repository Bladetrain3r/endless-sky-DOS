# Flight sprite and native mask alignment

Run `python3 dos/flight/test_alignment.py` inside the cached
`modern-arena-tools:0.1` Docker image after staging `assets.py`. The test reads
the six actual `ESSPRT1` files in `.work/flight/run`, the original PNGs, and
the frozen native `ESMASK1` geometry in `.work/collision/masks.txt`. It writes
`dos/reports/flight-alignment.json` and `.work/flight/alignment.png`. The
contact sheet shows blue staged coverage and orange original-mask boundaries.

Native `Drawable::Width/Height` supply the half-size dimension at unit zoom to
`DrawList::Push`; native `Mask::SmoothAndCenter` puts the original collision
outline in those world units. The DOS file contract therefore uses one world
pixel per two source PNG pixels. Its original-resolution canvas is even and a
multiple of four, with the source centered at an integer offset; the stored
canvas is half that size. This avoids a half-pixel center shift. The test
asserts those dimensions and independently decodes the ESSPRT1 alpha nibbles.
It reconstructs a reference alpha by rotating the original RGBA clockwise at
source resolution with Pillow bicubic and reducing with Lanczos. Comparing the
stored alpha with that reference tests frame order, direction, scale,
centering, and quantization, though it intentionally uses the specified
resampling contract rather than a native OpenGL screenshot.

Native `Mask::Contains` counts crossings across **all** outlines with odd/even
parity. Falcon's two inner outlines are holes, so the overlay applies XOR to
the separate polygons. The mask comparison samples 15 headings of each
rotating ship, including frames adjacent to cardinal angles, and Falcon's
static heading. It records coverage IoU and symmetric nearest-boundary mean,
95th percentile, and maximum distances. The mask intentionally differs from
the alpha silhouette: native tracing shifts by source alpha, then smooths and
simplifies the outline. A 3-world-pixel 95th-percentile boundary allowance
covers the native simplification threshold (1 world pixel) plus raster edge
and resampling uncertainty. Mean boundary distance must be at most 1.5 pixels
and IoU at least 0.75. Staged/reference extent may differ by at most 1 pixel,
centroid by 0.75 pixel, and alpha by 9/255 (the 4-bit quantizer's 8.5/255
maximum rounded to an integer). The first exploratory check used looser mask limits (2.5px mean, 5px p95,
0.70 IoU) and incorrectly filled Falcon’s holes; its Falcon comparison failed.
The corrected parity overlay passes the tighter limits above. This is a
regression/alignment check, not a preregistered statistical result.

The rebuilt assets pass all 33 sampled normal-alpha images. Maximum observed
staged/reference extent and centroid error are both zero; maximum alpha
error is 8/255. Across the 31 ship/mask comparisons, minimum IoU is 0.8104,
maximum mean boundary distance 0.606 px, 95th-percentile 2.0 px, and absolute
maximum 3.606 px. Sparrow and Barge each store 56×56 canvases from 70×80
source PNGs; their visible heading-zero extents are 34×38 and 28×38 world
pixels. Falcon stores 80×150 from 160×300, with a 72×143 visible extent.
At the nearest of 64 headings, the maximum angular quantization is 2.8125°;
for the approximately 20-pixel rotating-mask radius its additional possible
edge displacement is about 0.98 world pixel. The sampled overlay itself uses
matching headings and does not include that between-frame error.

Earth and Luna receive the same dimensional and reference-alpha checks.
Additive GLOW has no alpha plane in ESSPRT1, so only its size and file structure
are checked here. Palette RGB, runtime framebuffer placement, zoom, native
OpenGL output and every unsampled angle remain outside this qualification.
The original masks and PNGs are unchanged; their hashes and staged-file hashes
are in the report.
