#!/usr/bin/env python3
"""Stage native getter-derived stock Sparrow propulsion profile."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path
import struct
ROOT = Path(__file__).resolve().parents[2]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def stage(out):
    out = Path(out)
    work = ROOT / '.work/propulsion'
    provenance = json.loads((work / 'native-provenance.json').read_text())
    checks = [('dos/propulsion/oracle.cpp', 'oracle_source_sha256'),
              ('dos/propulsion/native.py', 'runner_source_sha256'),
              ('data/human/ships.txt', 'stock_ship_data_sha256'),
              ('data/human/weapons.txt', 'weapon_data_sha256')]
    for file, key in checks:
        assert digest(ROOT / file) == provenance[key], 'Rebuild propulsion native oracle'
    for file, expected in provenance['stock_outfit_data_sha256'].items():
        assert digest(ROOT / 'data/human' / file) == expected, 'Rebuild stock propulsion profile'
    for file, expected in provenance['behavior_source_sha256'].items():
        assert digest(ROOT / 'source' / file) == expected, 'Rebuild native behavioral reference'
    assert digest(work / 'native.csv') == provenance['csv_sha256']
    assert digest(work / 'PROPULSE.DAT') == provenance['profile_sha256']
    words = (work / 'PROPULSE.DAT').read_text().split()
    assert len(words) == 5 and words[0] == 'ESPROPULSION1'
    values = [float(word) for word in words[1:]]
    assert values == [provenance['stock_profile'][name] for name in
                      ('thrust_energy', 'thrust_heat', 'turn_energy', 'turn_heat')]
    out.mkdir(parents=True, exist_ok=True)
    target = out / 'PROPULSE.DAT'
    target.write_bytes(b'ESPROP2\0' + struct.pack('<4d', *values))
    return target
if __name__ == '__main__': stage(ROOT / '.work/flight/run')
