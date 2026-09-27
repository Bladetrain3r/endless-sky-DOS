#!/usr/bin/env python3
"""Stage exact native Energy Blaster damage getters for the DOS trainer."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def stage(out):
    out = Path(out)
    work = ROOT / '.work/damage'
    provenance = json.loads((work / 'native-provenance.json').read_text())
    assert digest(work / 'native.csv') == provenance['csv_sha256'], 'Rebuild damage native oracle'
    for file, key in [('dos/damage/oracle.cpp', 'oracle_source_sha256'),
                      ('dos/damage/native.py', 'runner_source_sha256'),
                      ('data/human/weapons.txt', 'weapon_data_sha256')]:
        assert digest(ROOT / file) == provenance[key], 'Rebuild damage native oracle'
    for file, expected in provenance['behavior_source_sha256'].items():
        assert digest(ROOT / 'source' / file) == expected, 'Rebuild native behavioral reference'
    stats = provenance['weapon']
    assert abs(stats['shield_damage'] - 10.6) < 1e-12
    assert abs(stats['hull_damage'] - 6.6) < 1e-12
    assert stats['hull_damage'] == stats['disabled_damage']
    out.mkdir(parents=True, exist_ok=True)
    profile = out / 'DAMAGE.DAT'
    profile.write_text(f"ESDAMAGE1\n{stats['shield_damage']:.17g} {stats['hull_damage']:.17g}\n")
    return profile

if __name__ == '__main__':
    stage(ROOT / '.work/flight/run')
