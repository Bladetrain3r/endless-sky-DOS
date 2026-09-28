#!/usr/bin/env python3
"""Stage the guarded, native-derived exact binary64 Star Barge profile."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[2]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def stage(out):
    work = ROOT / '.work/pursuit'
    p = json.loads((work / 'provenance.json').read_text())
    for file,key in [('dos/pursuit/oracle.cpp','oracle_source_sha256'),
                     ('dos/pursuit/native.py','runner_source_sha256'),
                     ('data/human/ships.txt','stock_ship_data_sha256')]:
        assert digest(ROOT/file) == p[key], 'Rebuild pursuit oracle'
    for file,expected in p['behavior_source_sha256'].items():
        assert digest(ROOT/'source'/file) == expected, 'Rebuild pursuit oracle'
    for file,expected in p['stock_outfit_data_sha256'].items():
        assert digest(ROOT/'data/human'/file) == expected, 'Rebuild pursuit oracle'
    for file,key in [('commands.csv','commands_sha256'),('intercepts.csv','intercepts_sha256'),
                     ('PURSUIT.DAT','profile_sha256')]:
        assert digest(work/file) == p[key], 'Rebuild pursuit oracle'
    data=(work/'PURSUIT.DAT').read_bytes()
    assert len(data)==48 and data[:8]==b'ESPURS1\0'
    values=struct.unpack('<5d',data[8:])
    assert values == tuple(p['profile_values'][k] for k in ('accel','reverse','turn','drag','mult'))
    out=Path(out); out.mkdir(parents=True,exist_ok=True)
    target=out/'PURSUIT.DAT'
    shutil.copyfile(work/'PURSUIT.DAT',target)
    return target
if __name__=='__main__': stage(ROOT/'.work/flight/run')
