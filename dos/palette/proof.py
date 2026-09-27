#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline palette experiment; not the runtime renderer or final asset compiler."""
import hashlib
import json
import re
from collections import Counter
from pathlib import Path
from PIL import Image, ImageDraw, __version__

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.work/palette'
# Intentionally bounded art sample: held-out assets do not train the palette.
TRAIN = ['ship/sparrow.png', 'ship/falcon.png', 'planet/earth.png',
         'planet/lava0.png', 'planet/forest0.png', 'land/canyon3.jpg']
HELD = ['ship/star barge.png', 'planet/lava3.png', 'land/water7.jpg']
UI = [(0, 0, 0), (255, 255, 255), (192, 192, 192), (48, 48, 48),
      (255, 64, 64), (64, 255, 96), (255, 208, 64), (64, 176, 255),
      (64, 240, 240), (208, 96, 255), (24, 32, 48), (96, 112, 144),
      (160, 64, 32), (32, 112, 64), (32, 64, 128), (112, 48, 128)]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name):
    # Normal alpha assets only: '=','+','^' need mode-aware conversion first.
    path = ROOT / 'images' / name
    if re.search(r'[=+^~][0-9]*$', path.stem) or '@sw' in path.stem:
        raise ValueError('Blend/swizzle mode not qualified: ' + name)
    im = Image.open(path).convert('RGBA')
    original = im.size
    im.thumbnail((192, 144), Image.Resampling.LANCZOS)
    return im, original


def dac(rgb):
    return tuple(round(c * 63 / 255) for c in rgb)


def expand(rgb):
    return tuple(round(c * 255 / 63) for c in rgb)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    samples = []
    for name in TRAIN:
        im, _ = load(name)
        # Equal cap per asset, exclude near-transparent pixels from training.
        colors = [p[:3] for p in im.getdata() if p[3] >= 128]
        stride = max(1, len(colors) // 4096)
        samples.extend(colors[::stride][:4096])
    strip = Image.new('RGB', (len(samples), 1))
    strip.putdata(samples)
    learned = strip.quantize(colors=208, method=Image.Quantize.MEDIANCUT)
    raw = learned.getpalette()[:208 * 3]
    colors = UI + [(round(i * 255 / 31),) * 3 for i in range(32)]
    colors += [tuple(raw[i:i + 3]) for i in range(0, len(raw), 3)]
    assert len(colors) == 256
    hardware = [dac(c) for c in colors]
    colors = [expand(c) for c in hardware]
    palette = Image.new('P', (1, 1))
    palette.putpalette([v for c in colors for v in c])
    (OUT / 'FLIGHT.PAL').write_bytes(bytes(v for c in hardware for v in c))
    palette_hash = digest(OUT / 'FLIGHT.PAL')
    rows = []
    sheet = Image.new('RGB', (800, 600), UI[10])
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 8), 'Reference resize | 256-color / 4-bit alpha | error shown in report', fill='white')
    for n, name in enumerate(TRAIN + HELD):
        im, original = load(name)
        rgb = im.convert('RGB')
        indexed = rgb.quantize(palette=palette, dither=Image.Dither.NONE)
        alpha = [round(a / 17) for a in im.getchannel('A').getdata()]
        packed = bytes(alpha[i] | ((alpha[i+1] if i+1 < len(alpha) else 0) << 4)
                       for i in range(0, len(alpha), 2))
        prefix = f'A{n:03d}'
        (OUT / (prefix + '.IDX')).write_bytes(indexed.tobytes())
        (OUT / (prefix + '.A4')).write_bytes(packed)
        decoded = [(packed[i // 2] >> (4 * (i % 2))) & 15 for i in range(len(alpha))]
        assert decoded == alpha
        # No transparent color key: opaque black remains drawable.
        rebuilt = indexed.convert('RGBA')
        a = Image.new('L', im.size)
        a.putdata([v * 17 for v in decoded])
        rebuilt.putalpha(a)
        backdrop = Image.new('RGBA', im.size, (*UI[10], 255))
        ref = Image.alpha_composite(backdrop, im).convert('RGB')
        result = Image.alpha_composite(backdrop, rebuilt).convert('RGB')
        # Actual final display must also use the same palette after blending.
        result = result.quantize(palette=palette, dither=Image.Dither.NONE).convert('RGB')
        error = sum(abs(a-b) for p,q in zip(ref.getdata(), result.getdata())
                    for a,b in zip(p,q)) / (im.width * im.height * 3)
        x, y = (n % 3) * 264 + 8, (n // 3) * 178 + 32
        # Contact-sheet thumbnails only; packed assets retain the sizes above.
        for pic, dx in [(ref, 0), (result, 128)]:
            pic.thumbnail((124, 140), Image.Resampling.NEAREST)
            sheet.paste(pic, (x + dx, y + 18))
        draw.text((x, y), ('T ' if name in TRAIN else 'H ') + name, fill='white')
        rows.append(dict(source='images/' + name, sha256=digest(ROOT/'images'/name),
                         training=name in TRAIN, original_size=original,
                         packed_size=im.size, prefix=prefix, mean_abs_channel_error=round(error, 3),
                         index_bytes=im.width*im.height, alpha_bytes=len(packed),
                         rgba_bytes=im.width*im.height*4))
    # Keep reference columns true-color; never quantize the comparison itself.
    sheet.save(OUT/'comparison.png')
    inventory = Counter(p.suffix.lower() for p in (ROOT/'images').rglob('*') if p.is_file())
    report = dict(schema=1, pillow=__version__, palette_sha256=palette_hash,
                  script_sha256=digest(Path(__file__)), copyright_sha256=digest(ROOT/'copyright'),
                  palette_entries=256, ui_entries=16, gray_entries=32, learned_entries=208,
                  dac_channel_bits=6, framebuffer_bytes_800x600=480000,
                  all_image_extensions=dict(sorted(inventory.items())), samples=rows,
                  limits=['Curated sample only; no animation, additive, premultiplied or swizzle proof.',
                          'Host conversion, not DOS render performance or gameplay equivalence.',
                          'Asset thumbnail limits are experimental, not runtime logical dimensions.'])
    (OUT/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(assets=len(rows), palette_sha256=palette_hash,
                         output=str(OUT), pillow=__version__)))


if __name__ == '__main__':
    main()
