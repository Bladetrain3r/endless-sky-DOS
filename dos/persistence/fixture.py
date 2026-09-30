#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test-only v1 envelope from the bounded native trace; NOT a native-save importer."""
import csv
import hashlib
import json
from pathlib import Path
import struct
import uuid
import zlib
ROOT=Path(__file__).resolve().parents[2]
def native_row(phase,content,generation):
    rows=list(csv.DictReader((ROOT/'dos/persistence/native-trace.csv').open()))
    row=next(r for r in rows if r['phase']==phase)
    limits=json.loads((ROOT/'dos/persistence/native-limits.json').read_text())
    text=lambda s:struct.pack('<H',len(s.encode()))+s.encode()
    chunk=lambda t,b:struct.pack('<III',t,1,len(b))+b
    ship=uuid.UUID(row['ship_uuid']).bytes
    pilot=bytes(range(1,17))
    p=pilot+text('Native fixture')+struct.pack('<3i',*limits['date'])
    p+=text('system:'+row['system'])+text('planet:'+row['planet'])+ship
    s=ship+text('ship:Sparrow')+text('')+text('Fixture Sparrow')
    s+=struct.pack('<i3d',limits['required_crew'],limits['fuel'],float(row['shields']),float(row['hull']))
    ledger=struct.pack('<qI',int(row['credits']),1)+text('commodity:Food')+struct.pack('<iq',int(row['qty']),int(row['basis']))
    sale=int(row['pending_sales'])
    scope=struct.pack('<II',0,int(bool(sale)))
    if sale:scope+=text('system:Sol')+text('commodity:Food')+struct.pack('<i',sale)
    payload=chunk(1,p)+chunk(2,s)+chunk(3,ledger)+chunk(4,scope)
    header=b'ESDSAVE1'+struct.pack('<IIQI',1,64,generation,len(payload))+content
    return header+struct.pack('<I',zlib.crc32(header+payload))+payload
