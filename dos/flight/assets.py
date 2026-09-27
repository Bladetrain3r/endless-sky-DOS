#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build bounded, fixed-palette flight prototype sprites and shared blend tables."""
import hashlib
import json
import math
from pathlib import Path
import struct
from PIL import Image, __version__

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'.work/flight/run'
SOURCES = [('SPARROW.SPR', 'ship/sparrow.png', 16, 0),
           ('BARGE.SPR', 'ship/star barge.png', 16, 0),
           ('FALCON.SPR', 'ship/falcon.png', 1, 0),
           ('EARTH.SPR', 'planet/earth.png', 1, 0),
           ('LUNA.SPR', 'planet/luna.png', 1, 0),
           ('GLOW.SPR', 'effect/blaster impact+0.png', 1, 1)]


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    raw = (ROOT/'.work/palette/FLIGHT.PAL').read_bytes()
    assert len(raw) == 768 and max(raw) <= 63
    (OUT/'FLIGHT.PAL').write_bytes(raw)
    colors = [tuple(round(v*255/63) for v in raw[i:i+3]) for i in range(0, 768, 3)]
    palette = Image.new('P', (1, 1))
    palette.putpalette([v for c in colors for v in c])
    rows = []
    for filename, name, frames, mode in SOURCES:
        path = ROOT/'images'/name
        source = Image.open(path).convert('RGBA')
        if frames > 1:
            side = math.ceil(math.hypot(*source.size)) + 2
            base = Image.new('RGBA', (side, side))
            base.paste(source, ((side-source.width)//2, (side-source.height)//2))
        else:
            base = source
        assert max(base.size) <= 512
        w, h = base.size
        payload = bytearray(b'ESSPRT1\0' + struct.pack('<HHHH', w, h, frames, mode))
        for frame in range(frames):
            # Clockwise screen angles agree with Angle::Unit's up=0 convention.
            im = base.rotate(-frame*360/frames, resample=Image.Resampling.BICUBIC)
            if mode == 1:
                # '+' is straight source alpha -> premultiplied additive RGB.
                rgb = Image.new('RGB', im.size)
                rgb.putdata([tuple(c*a//255 for c in (r,g,b)) for r,g,b,a in im.getdata()])
            else:
                rgb = im.convert('RGB')
            indexed = rgb.quantize(palette=palette, dither=Image.Dither.NONE).tobytes()
            if mode == 1:
                indexed = bytes(0 if colors[c] == (0,0,0) else c for c in indexed)
            payload.extend(indexed)
            if mode == 0:
                alpha = [round(a/17) for a in im.getchannel('A').getdata()]
                payload.extend(bytes(alpha[i] | ((alpha[i+1] if i+1 < len(alpha) else 0)<<4)
                                     for i in range(0, len(alpha), 2)))
        (OUT/filename).write_bytes(payload)
        rows.append(dict(file=filename, source='images/'+name, source_sha256=digest(path),
                         original_size=source.size, stored_size=base.size, frames=frames,
                         mode='premultiplied-additive' if mode else 'normal-alpha4',
                         disk_bytes=len(payload), decoded_bytes=w*h*frames*(1 if mode else 2),
                         sha256=digest(OUT/filename)))
    tables = bytearray()
    for level in range(16):
        if level == 0:
            tables.extend(bytes(range(256))*256)
        elif level == 15:
            tables.extend(b''.join(bytes([i])*256 for i in range(256)))
        else:
            im = Image.new('RGB', (256, 256))
            im.putdata([tuple((s*level+d*(15-level)+7)//15 for s,d in zip(src,dst))
                        for src in colors for dst in colors])
            tables.extend(im.quantize(palette=palette, dither=Image.Dither.NONE).tobytes())
    (OUT/'BLEND.LUT').write_bytes(tables)
    im = Image.new('RGB', (256, 256))
    im.putdata([tuple(min(255,s+d) for s,d in zip(src,dst)) for src in colors for dst in colors])
    additive = im.quantize(palette=palette, dither=Image.Dither.NONE).tobytes()
    # Zero source must leave destination identity intact despite duplicate palette entries.
    additive = bytes(range(256)) + additive[256:]
    (OUT/'ADD.LUT').write_bytes(additive)
    assert len(tables)==1048576 and len(additive)==65536
    assert tables[:65536] == bytes(range(256))*256
    assert all(tables[15*65536+s*256+d] == s for s in range(256) for d in range(256))
    report = dict(schema=1, pillow=__version__, sprites=rows, palette_sha256=digest(OUT/'FLIGHT.PAL'),
                  tables={name:dict(bytes=(OUT/name).stat().st_size, sha256=digest(OUT/name))
                          for name in ('BLEND.LUT','ADD.LUT')},
                  copyright_sha256=digest(ROOT/'copyright'), script_sha256=digest(Path(__file__)),
                  limitations=['16 prebaked headings, no runtime rotation qualification.',
                               'No collision geometry changed; prototype has no collision simulation.',
                               'Only normal PNG and one straight-alpha additive frame qualified.',
                               'All asset attribution remains in upstream copyright; local experiment only.'])
    (OUT/'assets.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(sprites=len(rows), decoded_sprite_bytes=sum(r['decoded_bytes'] for r in rows),
                         shared_lut_bytes=len(tables)+len(additive))))


if __name__ == '__main__':
    main()
