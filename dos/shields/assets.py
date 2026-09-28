#!/usr/bin/env python3
"""Stage guarded native getter-derived Sparrow shield profile as exact binary64."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path
import struct
ROOT = Path(__file__).resolve().parents[2]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def stage(out):
    out = Path(out)
    work = ROOT / '.work/shields'
    provenance = json.loads((work / 'native-provenance.json').read_text())
    checks = [('dos/shields/oracle.cpp', 'oracle_source_sha256'),
              ('dos/shields/native.py', 'runner_source_sha256'),
              ('data/human/ships.txt', 'stock_ship_data_sha256')]
    for file, key in checks:
        assert digest(ROOT / file) == provenance[key], 'Rebuild shield native oracle'
    for file, expected in provenance['stock_outfit_data_sha256'].items():
        assert digest(ROOT / 'data/human' / file) == expected, 'Rebuild shield profile'
    for file, expected in provenance['behavior_source_sha256'].items():
        assert digest(ROOT / 'source' / file) == expected, 'Rebuild native behavioral reference'
    assert digest(work / 'native.csv') == provenance['csv_sha256']
    assert digest(work / 'SHIELD.DAT') == provenance['profile_sha256']
    fields = (work / 'SHIELD.DAT').read_text().split()
    assert fields[0] == 'ESSHIELD1' and len(fields) == 10
    values = [float(v) for v in fields[1:8]]
    delays = [int(v) for v in fields[8:10]]
    out.mkdir(parents=True, exist_ok=True)
    target = out / 'SHIELD.DAT'
    target.write_bytes(b'ESSHLD2\0' + struct.pack('<7d2I', *values, *delays))
    stock = provenance['stock_profile']
    (out / 'PLAYER.DAT').write_bytes(b'ESPLAYER1' + struct.pack('<2d', stock['max_hull'], stock['minimum_hull']))
    return target
if __name__ == '__main__': stage(ROOT / '.work/flight/run')
