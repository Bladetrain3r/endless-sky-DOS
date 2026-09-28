#!/usr/bin/env python3
"""Stage guarded native-derived Star Barge profiles as exact binary64 values."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/opponent_profile'

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def stage(out):
    """Return {resource, shield, propulsion, hull} Path values after guarded staging."""
    provenance = json.loads((WORK / 'provenance.json').read_text())
    if provenance['source_commit'] != (ROOT / '.work/native/source-commit.txt').read_text().strip():
        raise ValueError('Rebuild opponent native oracle: source baseline changed')
    if digest(ROOT / '.work/native/build/endless-sky') != provenance['baseline_binary_sha256']:
        raise ValueError('Rebuild opponent native oracle: native binary baseline changed')
    checks = [('dos/opponent_profile/oracle.cpp', 'oracle_source_sha256'),
              ('dos/opponent_profile/native.py', 'runner_source_sha256'),
              ('data/human/ships.txt', 'stock_ship_data_sha256'),
              ('data/human/weapons.txt', 'weapon_data_sha256')]
    for file, key in checks:
        if digest(ROOT / file) != provenance[key]:
            raise ValueError(f'Rebuild opponent native oracle: {file} changed')
    for prefix, key in (('data/human', 'stock_outfit_data_sha256'),
                        ('source', 'behavior_source_sha256')):
        for file, expected in provenance[key].items():
            if digest(ROOT / prefix / file) != expected:
                raise ValueError(f'Rebuild opponent native oracle: {prefix}/{file} changed')
    if digest(WORK / 'native.csv') != provenance['csv_sha256']:
        raise ValueError('Rebuild opponent native oracle: trace changed')
    for file, expected in provenance['profile_sha256'].items():
        if digest(WORK / file) != expected:
            raise ValueError(f'Rebuild opponent native oracle: {file} changed')
    specs = {
        'resource': ('BARGERES.txt', 'ESRESOURCE1', 'BARGERES.DAT', b'ESRES2\0\0', '<7d'),
        'shield': ('BARGESH.txt', 'ESSHIELD1', 'BARGESH.DAT', b'ESSHLD2\0', '<7d2I'),
        'propulsion': ('BARGEPRO.txt', 'ESPROPULSION1', 'BARGEPRO.DAT', b'ESPROP2\0', '<4d'),
        'hull': ('BARGEHP.txt', 'ESPLAYER1', 'BARGEHP.DAT', b'ESPLAYER1', '<2d'),
    }
    payloads = {}
    for key, (source, header, _, magic, fmt) in specs.items():
        words = (WORK / source).read_text().split()
        if not words or words[0] != header or len(words) != 1 + len(struct.unpack(fmt, bytes(struct.calcsize(fmt)))):
            raise ValueError(f'Malformed native profile: {source}')
        values = [float(word) for word in words[1:]]
        if not all(math.isfinite(value) for value in values):
            raise ValueError(f'Nonfinite native profile: {source}')
        if key == 'shield':
            if any(value < 0 or not value.is_integer() for value in values[-2:]):
                raise ValueError('Invalid native shield delay')
            values[-2:] = [int(value) for value in values[-2:]]
        payloads[key] = magic + struct.pack(fmt, *values)
    hull = struct.unpack('<2d', payloads['hull'][9:])
    if hull != tuple(provenance['hull'][key] for key in ('max_hull', 'minimum_hull')):
        raise ValueError('Native hull profile mismatch')
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    result = {}
    for key, (_, _, filename, _, _) in specs.items():
        result[key] = out / filename
        result[key].write_bytes(payloads[key])
    return result

if __name__ == '__main__':
    print(stage(ROOT / '.work/flight/run'))
