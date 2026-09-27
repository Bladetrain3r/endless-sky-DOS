#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a typed immutable world snapshot from resolved native-loader JSON."""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

KINDS = {'systems': 1, 'planets': 2, 'commodities': 3}
MAX_RECORD = 32768
MAX_HEAP = 262144


def fnv(data, h=2166136261):
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h


def identity(kind, name):
    data = name.encode('utf-8')
    digest = hashlib.sha256(b'ESWORLD1\0' + struct.pack('<II', kind, len(data)) + data).digest()
    return int.from_bytes(digest[:8], 'little')


def string(value):
    if value is None:
        return struct.pack('<I', 0xffffffff)
    data = value.encode('utf-8')
    if b'\0' in data:
        raise ValueError('NUL in name/string')
    return struct.pack('<I', len(data)) + data


def counted(rows):
    return struct.pack('<I', len(rows)) + b''.join(rows)


def flags(row, names):
    if any(type(row[n]) is not bool for n in names):
        raise ValueError('flags must be booleans')
    return sum(int(row[n]) << i for i, n in enumerate(names))


def encode(kind, r, ids):
    def ref(k, name):
        return ids[k, name] if name is not None else 0
    b = string(r['name'])
    if kind == 3:
        return b + struct.pack('<iii', r['ordinal'], r['low'], r['high'])
    b += string(r['display'])
    names = ('valid', 'hidden', 'shrouded', 'inaccessible') if kind == 1 else ('valid', 'inhabited')
    b += struct.pack('<I', flags(r, names))
    if kind == 1:
        values = [r[k] for k in ('x', 'y', 'jump_range', 'arrival_hyper', 'arrival_jump',
                                'departure_hyper', 'departure_jump', 'habitable')]
        if not all(math.isfinite(v) for v in values):
            raise ValueError('nonfinite system coordinate')
        b += struct.pack('<8d', *values)
    b += counted([string(a) for a in r['attributes']])
    if kind == 2:
        return b + counted([struct.pack('<Q', ref(1, s)) for s in r['systems']])
    if len(set(r['links'])) != len(r['links']):
        raise ValueError('duplicate links')
    b += counted([struct.pack('<Q', ref(1, s)) for s in r['links']])
    objects = []
    for i, o in enumerate(r['objects']):
        if o['index'] != i or not (-1 <= o['parent'] < i):
            raise ValueError('invalid orbit tree')
        if not all(math.isfinite(o[k]) for k in ('distance', 'period', 'offset')):
            raise ValueError('nonfinite orbit')
        objects.append(struct.pack('<iiQddd', i, o['parent'], ref(2, o['planet']),
                                   o['distance'], o['period'], o['offset']) + string(o['sprite']))
    b += counted(objects)
    if len({t['commodity'] for t in r['trade']}) != len(r['trade']):
        raise ValueError('duplicate commodity prices')
    return b + counted([struct.pack('<Qi', ref(3, t['commodity']), t['price']) for t in r['trade']])


class Cursor:
    def __init__(self, data):
        self.data, self.at = data, 0

    def read(self, fmt):
        n = struct.calcsize('<' + fmt)
        if self.at + n > len(self.data):
            raise ValueError('truncated field')
        result = struct.unpack_from('<' + fmt, self.data, self.at)
        self.at += n
        return result[0] if len(result) == 1 else result

    def text(self, nullable=False):
        n = self.read('I')
        if nullable and n == 0xffffffff:
            return None
        if self.at + n > len(self.data):
            raise ValueError('truncated string')
        s = self.data[self.at:self.at+n].decode('utf-8')
        self.at += n
        if '\0' in s:
            raise ValueError('NUL string')
        return s


def decode(kind, data, names):
    c = Cursor(data)
    r = {'name': c.text()}
    if kind == 3:
        r.update(zip(('ordinal', 'low', 'high'), c.read('iii')))
    else:
        r['display'] = c.text()
        f = c.read('I')
        fields = ('valid', 'hidden', 'shrouded', 'inaccessible') if kind == 1 else ('valid', 'inhabited')
        r.update({n: bool(f & (1 << i)) for i, n in enumerate(fields)})
        if kind == 1:
            r.update(zip(('x', 'y', 'jump_range', 'arrival_hyper', 'arrival_jump',
                          'departure_hyper', 'departure_jump', 'habitable'), c.read('8d')))
        r['attributes'] = [c.text() for _ in range(c.read('I'))]
        if kind == 2:
            r['systems'] = [names[1, c.read('Q')] for _ in range(c.read('I'))]
        else:
            r['links'] = [names[1, c.read('Q')] for _ in range(c.read('I'))]
            r['objects'] = []
            for _ in range(c.read('I')):
                i, p, planet, d, period, offset = c.read('iiQddd')
                r['objects'].append(dict(index=i, parent=p, planet=names[2, planet] if planet else None,
                                         sprite=c.text(True), distance=d, period=period, offset=offset))
            r['trade'] = []
            for _ in range(c.read('I')):
                cid, price = c.read('Qi')
                r['trade'].append(dict(commodity=names[3, cid], price=price))
    if c.at != len(data):
        raise ValueError('trailing bytes')
    return r


def build(source, output):
    if source['schema'] != 1:
        raise ValueError('unknown native dump schema')
    ids, used, rows = {}, set(), []
    for group, kind in KINDS.items():
        for r in source[group]:
            ident = identity(kind, r['name'])
            if not ident or ident in used or (kind, r['name']) in ids:
                raise ValueError('duplicate or colliding identity')
            ids[kind, r['name']] = ident
            used.add(ident)
            rows.append((ident, kind, r))
    rows.sort(key=lambda row: row[0])
    names = {(kind, ident): name for (kind, name), ident in ids.items()}
    payloads = [encode(kind, row, ids) for _, kind, row in rows]
    largest = max(map(len, payloads), default=0)
    count = len(rows)
    tracked = count * 24 + largest
    if not count or count > 8192 or largest > MAX_RECORD or tracked > MAX_HEAP:
        raise ValueError('content reader budget exceeded')
    # Independent decoding checks every field and link, including binary64 values.
    for (_, kind, original), data in zip(rows, payloads):
        if decode(kind, data, names) != original:
            raise ValueError('typed roundtrip differs from native row: ' + original['name'])
    at = 24 + count * 24
    directory = []
    total_hash = 2166136261
    for (ident, kind, _), data in zip(rows, payloads):
        directory.append(struct.pack('<QIIII', ident, kind, at, len(data), fnv(data)))
        at += len(data)
        total_hash = fnv(data, total_hash)
    result = b'ESWORLD1' + struct.pack('<IIII', count, largest, 24+count*24, at)
    result += b''.join(directory) + b''.join(payloads)
    output.write_bytes(result)
    counts = {k: len(source[k]) for k in KINDS}
    counts.update(records=count, links=sum(len(s['links']) for s in source['systems']),
                  objects=sum(len(s['objects']) for s in source['systems']),
                  trades=sum(len(s['trade']) for s in source['systems']),
                  memberships=sum(len(p['systems']) for p in source['planets']))
    return dict(schema=1, counts=counts, pack_bytes=len(result), directory_bytes=count*24,
                maximum_record_bytes=largest, tracked_heap_bytes=tracked,
                payload_hash_hex=f'{total_hash:08x}', sha256=hashlib.sha256(result).hexdigest(),
                roundtrip='all native fields and references exact',
                identities=[dict(id=f'{i:016x}', kind=k, name=r['name']) for i,k,r in rows])


def main():
    p = argparse.ArgumentParser()
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    report = build(json.loads(args.source.read_text()), args.output)
    report['native_json_sha256'] = hashlib.sha256(args.source.read_bytes()).hexdigest()
    report['builder_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'identities'}, indent=2))


if __name__ == '__main__':
    main()
