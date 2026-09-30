#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a bounded native Sol permission/description snapshot and PNG radii."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.work/flight/run'
SOURCE=ROOT/'.work/navigation/PLANETS.json'

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    native=json.loads(SOURCE.read_text())
    assert native['schema']==1 and native['ship']=='Sparrow'
    assert 0<native['landing_speed']<=1 and 0<=native['idle_heat']<18600
    fields=[('can_land',1),('can_use_services',2),('has_services',4),
            ('has_outfitter',8),('has_shipyard',16),('can_recharge_hull',32),
            ('can_recharge_shields',64),('can_recharge_energy',128)]
    result=bytearray(b'ESNAV1\0\0'+struct.pack('<2d',native['idle_heat'],native['landing_speed']))
    rows=[]
    for name in ('Earth','Luna'):
        p=next(p for p in native['planets'] if p['name']==name)
        image=ROOT/'images/planet'/f'{name.lower()}.png'
        with Image.open(image) as im: dimensions=im.size
        # Native Drawable width/height = source PNG /2, StellarObject radius
        # = min(width,height)/2. Do not infer radius from padded runtime canvases.
        radius=min(dimensions)/4.
        description=p['description'].replace('\t',' ').translate(str.maketrans({'’':"'",'‘':"'",'“':'"','”':'"','—':'-','–':'-'})).encode('ascii')
        assert len(description)<1024 and b'\0' not in description
        flags=sum(bit for field,bit in fields if p[field])
        result+=name.encode().ljust(32,b'\0')+description.ljust(1024,b'\0')+struct.pack('<Id',flags,radius)
        rows.append(dict(name=name,radius=radius,flags=flags,image_sha256=digest(image)))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'NAV.DAT').write_bytes(result)
    shutil.copyfile(ROOT/'.work/navigation/APPROACH.DAT',OUT/'APPROACH.DAT')
    (OUT/'navigation.json').write_text(json.dumps(dict(native_snapshot_sha256=digest(SOURCE),
        source_sha256=digest(Path(__file__)),native_provenance_sha256=digest(ROOT/'.work/navigation/provenance.json'),
        planets=rows,bytes=len(result),sha256=digest(OUT/'NAV.DAT'),
        scope='Native initial-politics snapshot, Earth/Luna only; descriptions retain upstream license. No dynamic campaign politics.'),indent=2)+'\n')
    print(json.dumps({'navigation_bytes':len(result),'planets':rows}))
if __name__=='__main__': main()
