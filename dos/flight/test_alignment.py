#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent physical scale and frozen-mask alignment check for staged ESSPRT1."""
import hashlib
import json
import math
from pathlib import Path
import struct

from PIL import Image, ImageChops, ImageDraw, __version__ as pillow_version

ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / '.work/flight/run'
MASKS = ROOT / '.work/collision/masks.txt'
REPORT = ROOT / 'dos/reports/flight-alignment.json'
SHEET = ROOT / '.work/flight/alignment.png'
SOURCES = {
    'SPARROW.SPR': 'images/ship/sparrow.png',
    'BARGE.SPR': 'images/ship/star barge.png',
    'FALCON.SPR': 'images/ship/falcon.png',
    'EARTH.SPR': 'images/planet/earth.png',
    'LUNA.SPR': 'images/planet/luna.png',
    'GLOW.SPR': 'images/effect/blaster impact+0.png',
}
SHIP_IDS = {'SPARROW.SPR': 0, 'BARGE.SPR': 1, 'FALCON.SPR': 2}
SAMPLES = (0, 1, 7, 8, 15, 16, 17, 24, 31, 32, 33, 40, 48, 56, 63)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decode(path):
    blob = path.read_bytes()
    assert blob[:8] == b'ESSPRT1\0', path
    w, h, frames, mode = struct.unpack_from('<HHHH', blob, 8)
    assert 0 < w <= 512 and 0 < h <= 512 and frames in (1, 64) and mode in (0, 1)
    n = w * h
    stride = n + (0 if mode else (n + 1) // 2)
    assert len(blob) == 16 + frames * stride, (path, len(blob), stride)
    alphas = []
    for k in range(frames):
        start = 16 + k * stride + n
        if mode:
            alphas.append(None)
        else:
            packed = blob[start:start + (n + 1) // 2]
            values = bytearray(n)
            for i, value in enumerate(packed):
                values[2 * i] = (value & 15) * 17
                if 2 * i + 1 < n:
                    values[2 * i + 1] = (value >> 4) * 17
            alphas.append(Image.frombytes('L', (w, h), bytes(values)))
    return (w, h, frames, mode), alphas


def native_masks():
    words = MASKS.read_text().split()
    assert words[:2] == ['ESMASK1', '3']
    pos, result = 2, {}
    for _ in range(3):
        ident, outlines, points = map(int, words[pos:pos + 3])
        pos += 4  # fourth value is the original radius
        shape = []
        for _ in range(outlines):
            count = int(words[pos]); pos += 1
            poly = []
            for _ in range(count):
                poly.append((float(words[pos]), float(words[pos + 1])))
                pos += 2
            shape.append(poly)
        assert sum(map(len, shape)) == points
        result[ident] = shape
    assert pos == len(words)
    return result


def expected(source, size, heading):
    canvas = Image.new('RGBA', (size[0] * 2, size[1] * 2))
    x = (canvas.width - source.width) // 2
    y = (canvas.height - source.height) // 2
    assert (canvas.width - source.width) % 2 == 0
    assert (canvas.height - source.height) % 2 == 0
    canvas.paste(source, (x, y))
    return canvas.rotate(-heading * 360 / 64, resample=Image.Resampling.BICUBIC).resize(
        size, Image.Resampling.LANCZOS).getchannel('A')


def binary(alpha):
    return alpha.point(lambda a: 255 if a >= 128 else 0)


def bbox_extent(mask):
    box = mask.getbbox()
    assert box is not None
    return [box[2] - box[0], box[3] - box[1]]


def centroid(mask):
    w, h = mask.size
    data = mask.tobytes()
    indices = [i for i, value in enumerate(data) if value]
    assert indices
    return [(sum(i % w for i in indices) / len(indices)) - (w - 1) / 2,
            (sum(i // w for i in indices) / len(indices)) - (h - 1) / 2]


def boundary(mask):
    w, h = mask.size
    pix = mask.load()
    return [(x, y) for y in range(h) for x in range(w) if pix[x, y] and
            (x == 0 or x == w - 1 or y == 0 or y == h - 1 or
             not all(pix[nx, ny] for nx, ny in ((x - 1, y), (x + 1, y),
                                                (x, y - 1), (x, y + 1))))]


def mask_image(size, outlines, heading):
    im = Image.new('L', size)
    theta = math.radians(heading * 360 / 64)
    c, s = math.cos(theta), math.sin(theta)
    for shape in outlines:
        points = [(size[0] / 2 + c * x - s * y,
                   size[1] / 2 + s * x + c * y) for x, y in shape]
        layer = Image.new('L', size)
        ImageDraw.Draw(layer).polygon(points, fill=255)
        im = ImageChops.difference(im, layer)
    return im


def separation(a, b):
    """Boundary nearest-neighbor distances in both directions, Euclidean pixels."""
    aa, bb = boundary(a), boundary(b)
    assert aa and bb
    def distances(one, other):
        return sorted(math.sqrt(min((x - xx) ** 2 + (y - yy) ** 2 for xx, yy in other))
                      for x, y in one)
    d = distances(aa, bb) + distances(bb, aa)
    d.sort()
    return round(sum(d) / len(d), 3), round(d[math.ceil(.95 * len(d)) - 1], 3), round(d[-1], 3)


def iou(a, b):
    aa, bb = a.tobytes(), b.tobytes()
    intersection = sum(bool(x) and bool(y) for x, y in zip(aa, bb))
    union = sum(bool(x) or bool(y) for x, y in zip(aa, bb))
    return round(intersection / union, 4)


def contact(items):
    cols, rows, cell_w, cell_h = 4, math.ceil(len(items) / 4), 250, 220
    sheet = Image.new('RGB', (cols * cell_w, rows * cell_h), (28, 31, 39))
    pen = ImageDraw.Draw(sheet)
    for index, (name, heading, alpha, mask) in enumerate(items):
        left, top = (index % cols) * cell_w, (index // cols) * cell_h
        colored = Image.new('RGB', alpha.size, (34, 38, 46))
        colored.paste((115, 177, 228), mask=binary(alpha))
        if mask is not None:
            edge = Image.new('RGBA', alpha.size)
            p = ImageDraw.Draw(edge)
            for x, y in boundary(mask):
                p.point((x, y), fill=(255, 119, 72, 255))
            colored.paste(edge, mask=edge.getchannel('A'))
        sheet.paste(colored, (left + (cell_w - alpha.width) // 2,
                              top + 27 + (cell_h - 35 - alpha.height) // 2))
        pen.text((left + 8, top + 6), f'{name}  {heading * 360 / 64:g} deg', fill=(245, 245, 245))
    SHEET.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(SHEET)


def main():
    masks = native_masks()
    rows, images, failures = [], [], []
    # One source pixel must become exactly half a world pixel. The one-pixel
    # staged/reference extent threshold covers 4-bit rounding at a hard edge.
    limits = {'extent_px': 1, 'centroid_px': .75, 'alpha_error': 9,
              'mask_mean_boundary_px': 1.5, 'mask_p95_boundary_px': 3.0,
              'mask_iou': .75}
    for filename, source_name in SOURCES.items():
        source_path, stage_path = ROOT / source_name, STAGE / filename
        source = Image.open(source_path).convert('RGBA')
        (w, h, frames, mode), frames_alpha = decode(stage_path)
        rotating = filename in ('SPARROW.SPR', 'BARGE.SPR')
        if rotating:
            minimum = math.hypot(*source.size) + 4
            assert w == h and 2 * w >= minimum and 2 * w < minimum + 4
        else:
            assert 2 * w >= source.width and 2 * w < source.width + 4
            assert 2 * h >= source.height and 2 * h < source.height + 4
        assert (2 * w) % 4 == (2 * h) % 4 == 0
        assert (2 * w - source.width) % 2 == (2 * h - source.height) % 2 == 0
        assert frames == (64 if rotating else 1)
        assert mode == (1 if filename == 'GLOW.SPR' else 0)
        samples = SAMPLES if rotating else (0,)
        for heading in samples:
            ref = expected(source, (w, h), heading)
            if mode:
                # Additive ESSPRT1 has no alpha plane; canvas/scale is checked.
                continue
            actual = frames_alpha[heading]
            errors = [abs(x - y) for x, y in zip(actual.tobytes(), ref.tobytes())]
            max_error = max(errors)
            a, b = binary(actual), binary(ref)
            ea, eb = bbox_extent(a), bbox_extent(b)
            extent_error = max(abs(x - y) for x, y in zip(ea, eb))
            ca, cb = centroid(a), centroid(b)
            centroid_error = math.dist(ca, cb)
            row = {'sprite': filename, 'heading': heading,
                   'degrees_clockwise': heading * 360 / 64,
                   'source_extent_px': list(source.size), 'stored_canvas_px': [w, h],
                   'rendered_extent_px': ea, 'reference_extent_px': eb,
                   'extent_error_px': extent_error,
                   'centroid_offset_from_canvas_px': [round(c, 3) for c in ca],
                   'centroid_error_px': round(centroid_error, 3),
                   'max_alpha_error_8bit': max_error}
            if filename in SHIP_IDS:
                poly = mask_image((w, h), masks[SHIP_IDS[filename]], heading)
                mean, p95, maximum = separation(a, poly)
                row.update(mask_mean_boundary_px=mean, mask_p95_boundary_px=p95,
                           mask_max_boundary_px=maximum, mask_iou=iou(a, poly))
                if mean > limits['mask_mean_boundary_px'] or p95 > limits['mask_p95_boundary_px'] or row['mask_iou'] < limits['mask_iou']:
                    failures.append(f'{filename} heading {heading}: mask/alpha alignment')
            else:
                poly = None
            if max_error > limits['alpha_error'] or extent_error > limits['extent_px'] or centroid_error > limits['centroid_px']:
                failures.append(f'{filename} heading {heading}: staged/reference alpha')
            rows.append(row)
            if filename in SHIP_IDS:
                images.append((filename.replace('.SPR', ''), heading, actual, poly))
    contact(images)
    report = {'schema': 1, 'pillow': pillow_version,
              'contract': 'Original RGBA centered on even multiple-of-four canvas, clockwise bicubic source rotation, Lanczos half resize; native masks unchanged.',
              'tolerances': limits, 'inputs': {'masks_sha256': sha(MASKS),
              'test_sha256': sha(Path(__file__)),
              'sprites': {name: {'source_sha256': sha(ROOT / path),
                                  'stage_sha256': sha(STAGE / name)} for name, path in SOURCES.items()}},
              'observations': rows, 'pass': not failures, 'failures': failures,
              'contact_sheet': str(SHEET.relative_to(ROOT)),
              'limitations': ['Source/resampled alpha is checked independently of palette RGB; additive GLOW has no stored alpha plane.',
                              'Mask polygon differs deliberately from alpha after native smoothing and simplification; the sampled comparison is geometric, not pixel equality.',
                              'Fifteen of 64 rotating headings sampled, including adjacent/intermediate frames; no runtime framebuffer or native OpenGL screenshot is compared.']}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'pass': report['pass'], 'samples': len(rows), 'failures': failures,
                      'mask_min_iou': min(r['mask_iou'] for r in rows if 'mask_iou' in r),
                      'mask_max_p95_px': max(r['mask_p95_boundary_px'] for r in rows if 'mask_p95_boundary_px' in r)}))
    if failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
