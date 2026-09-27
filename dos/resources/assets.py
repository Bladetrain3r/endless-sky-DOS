#!/usr/bin/env python3
"""Stage native getter-derived Sparrow + Energy Blaster resource profile."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path
import shutil
ROOT = Path(__file__).resolve().parents[2]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def stage(out):
    out = Path(out)
    work = ROOT / '.work/resources'
    provenance = json.loads((work / 'native-provenance.json').read_text())
    checks = [('dos/resources/oracle.cpp', 'oracle_source_sha256'),
              ('dos/resources/native.py', 'runner_source_sha256'),
              ('data/human/ships.txt', 'stock_ship_data_sha256'),
              ('data/human/weapons.txt', 'weapon_data_sha256')]
    for file, key in checks:
        assert digest(ROOT / file) == provenance[key], 'Rebuild resource native oracle'
    for file, expected in provenance['stock_outfit_data_sha256'].items():
        assert digest(ROOT / 'data/human' / file) == expected, 'Rebuild stock resource profile'
    for file, expected in provenance['behavior_source_sha256'].items():
        assert digest(ROOT / 'source' / file) == expected, 'Rebuild native behavioral reference'
    assert digest(work / 'native.csv') == provenance['csv_sha256']
    assert digest(work / 'RESOURCE.DAT') == provenance['profile_sha256']
    out.mkdir(parents=True, exist_ok=True)
    target = out / 'RESOURCE.DAT'
    shutil.copyfile(work / 'RESOURCE.DAT', target)
    return target
if __name__ == '__main__': stage(ROOT / '.work/flight/run')
