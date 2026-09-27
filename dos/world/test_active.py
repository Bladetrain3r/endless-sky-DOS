#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native active-system API integration and corruption test."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from pack import identity, fnv

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/active'
PACK = ROOT / '.work/world/run/WORLD.PAK'
NATIVE = ROOT / '.work/world/native.json'
SRC = ROOT / 'dos/world'
WORK.mkdir(parents=True, exist_ok=True)
CC = os.environ.get('CC', 'cc')
subprocess.run([CC, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                str(SRC/'active.c'), str(SRC/'active_test.c'),
                '-o', str(WORK/'active_test')], check=True)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1')

def run(pack, name='Sol', other='Sirius'):
    return subprocess.run([str(WORK/'active_test'), str(pack), name, other],
                          text=True, capture_output=True, env=env, timeout=60)


def compare(name):
    result = run(PACK, name)
    assert result.returncode == 0, result.stderr
    lines = [x.split('\t') for x in result.stdout.splitlines()]
    native = json.loads(NATIVE.read_text())
    s = next(x for x in native['systems'] if x['name'] == name)
    hdr = lines.pop(0)
    assert hdr[:4] == ['S', name, s['display'], str(sum(int(s[k]) << i for i, k in enumerate(
        ('valid', 'hidden', 'shrouded', 'inaccessible'))))]
    vals = ('x', 'y', 'jump_range', 'arrival_hyper', 'arrival_jump',
            'departure_hyper', 'departure_jump', 'habitable')
    assert [float(x) for x in hdr[4:12]] == [s[k] for k in vals]
    assert [int(x) for x in hdr[12:]] == [len(s[k]) for k in ('objects', 'links', 'trade')]
    for expected in s['objects']:
        row = lines.pop(0)
        assert row[0] == 'O'
        assert [int(x) for x in row[1:3]] == [expected['index'], expected['parent']]
        assert int(row[3], 16) == (identity(2, expected['planet']) if expected['planet'] else 0)
        assert [float(x) for x in row[4:7]] == [expected[k] for k in ('distance', 'period', 'offset')]
        assert row[7] == (expected['sprite'] if expected['sprite'] is not None else '<NULL>')
    for link in s['links']:
        assert lines.pop(0) == ['L', f'{identity(1, link):016x}']
    for trade in s['trade']:
        assert lines.pop(0) == ['T', f"{identity(3, trade['commodity']):016x}", str(trade['price'])]
    assert not lines
    return s


def rejected(data, label):
    path = WORK / f'bad_{label}.pak'
    path.write_bytes(data)
    result = run(path)
    assert result.returncode == 2 and result.stdout.strip() == 'OPEN_REJECTED', (label, result)
    path.unlink()


sol = compare('Sol')
compare('Sirius')
native = json.loads(NATIVE.read_text())
scan = subprocess.run([str(WORK/'active_test'), str(PACK), '--scan'],
                      input=''.join(s['name']+'\n' for s in native['systems']),
                      text=True, capture_output=True, env=env, timeout=60)
assert scan.returncode == 0, scan.stderr
rows = scan.stdout.strip().split('\t')
assert rows[0] == 'SCAN' and int(rows[1]) == len(native['systems'])
assert int(rows[2]) <= 65536 and int(rows[3]) <= 262144

data = bytearray(PACK.read_bytes())
rejected(data[:-1], 'truncated')
bad = bytearray(data)
bad[-1] ^= 1
rejected(bad, 'checksum')
bad = bytearray(data)
struct.pack_into('<I', bad, 8, 8193)
rejected(bad, 'count')
bad = bytearray(data)
struct.pack_into('<I', bad, 16, 0)
rejected(bad, 'directory')
# Repair the checksum after changing Sol's first link to a planet ID.
entries = [struct.unpack_from('<QIIII', data, 24+i*24) for i in range(struct.unpack_from('<I', data, 8)[0])]
sol_id = identity(1, 'Sol')
planet_id = identity(2, 'Earth')
ix, entry = next((i,e) for i,e in enumerate(entries) if e[0] == sol_id)
_,_,off,length,_ = entry
at = off
for _ in range(2):
    n, = struct.unpack_from('<I', data, at); at += 4+n
at += 4+8*8
attrs, = struct.unpack_from('<I', data, at); at += 4
for _ in range(attrs):
    n, = struct.unpack_from('<I', data, at);at += 4+n
links, = struct.unpack_from('<I', data, at);at += 4
assert links == len(sol['links'])
bad = bytearray(data)
struct.pack_into('<Q', bad, at, planet_id)
struct.pack_into('<I', bad, 24+ix*24+20, fnv(bad[off:off+length]))
rejected(bad, 'wrong_kind_reference')
print('active systems: Sol/Sirius JSON fields, dual ownership, 100 reloads each, all systems loaded, five corruptions passed')

report = dict(status='pass', systems_loaded=int(rows[1]), maximum_active_bytes=int(rows[2]),
              store_bytes=int(rows[3]), abi='native host; DOS struct sizes differ',
              field_comparisons=['Sol','Sirius'], ownership='concurrent, repeated, survives store close',
              corruptions_rejected=5, sanitizer='ASan+UBSan; leak detection disabled',
              pack_sha256=hashlib.sha256(PACK.read_bytes()).hexdigest(),
              sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                       [SRC/'active.c',SRC/'active.h',SRC/'active_test.c',Path(__file__)]})
(ROOT/'dos/reports/active-world-baseline.json').write_text(json.dumps(report,indent=2)+'\n')
