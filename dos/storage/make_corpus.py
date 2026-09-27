#!/usr/bin/env python3
"""Byte-preserving record store, NOT a replacement Endless Sky semantic parser."""
from pathlib import Path
import hashlib
import json
import random
import sqlite3
import struct

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'.work/storage/run'


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def split_records(data):
    """Split at non-comment, unindented lines; retain every original byte."""
    starts, pos = [], 0
    for line in data.splitlines(keepends=True):
        content = line.removeprefix(b'\xef\xbb\xbf') if pos == 0 else line
        if content and content[0] > 32 and not content.startswith(b'#'):
            starts.append(pos)
        pos += len(line)
    # Preamble travels with the first definition, including source notices.
    boundaries = [0] + starts[1:] + [len(data)]
    return [data[a:b] for a, b in zip(boundaries, boundaries[1:]) if b > a]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    records, provenance = [], []
    for path in sorted((ROOT/'data').rglob('*.txt')):
        source = path.read_bytes()
        chunks = split_records(source)
        assert b''.join(chunks) == source
        provenance.append({'path': str(path.relative_to(ROOT)),
                           'sha256': hashlib.sha256(source).hexdigest(),
                           'first_record': len(records), 'records': len(chunks)})
        records.extend(chunks)
    maximum = max(map(len, records))
    if maximum > 256*1024:
        raise RuntimeError('Record exceeds benchmark payload limit')
    dbpath = OUT/'CONTENT.DB'
    dbpath.unlink(missing_ok=True)
    with sqlite3.connect(dbpath) as db:
        db.execute('PRAGMA page_size=4096')
        db.execute('CREATE TABLE records(id INTEGER PRIMARY KEY, payload BLOB NOT NULL)')
        db.executemany('INSERT INTO records VALUES (?,?)', enumerate(records))
        db.commit()
        assert db.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
        assert [row[0] for row in db.execute('SELECT payload FROM records ORDER BY id')] == records
    start = 16 + len(records)*8
    index, offset = bytearray(), start
    for payload in records:
        index += struct.pack('<II', offset, len(payload))
        offset += len(payload)
    with (OUT/'CONTENT.PAK').open('wb') as pack:
        pack.write(b'ESPACK1\0'+struct.pack('<II',len(records),maximum))
        pack.write(index)
        for payload in records:
            pack.write(payload)
    # Independently decode the generated pack and compare exact source bytes.
    raw = (OUT/'CONTENT.PAK').read_bytes()
    for i, expected in enumerate(records):
        at, size = struct.unpack_from('<II',raw,16+8*i)
        assert raw[at:at+size] == expected
    rng = random.Random(27092026)
    hot = rng.sample(range(len(records)),16)
    traces = {'SEQ': list(range(len(records))),
              'RAND': [rng.randrange(len(records)) for _ in range(1024)],
              'HOT': [hot[i % len(hot)] for i in range(1024)]}
    info = {}
    for name, ids in traces.items():
        with (OUT/(name+'.BIN')).open('wb') as trace:
            trace.write(b'ESTRACE1'+struct.pack('<I',len(ids)))
            for i in ids:
                trace.write(struct.pack('<III',i,len(records[i]),fnv(records[i])))
        info[name] = {'queries':len(ids), 'payload_bytes':sum(len(records[i]) for i in ids)}
    summary = {'kind':'raw-content-records-not-parsed-world-state',
        'source_files':provenance, 'records':len(records), 'payload_bytes':sum(map(len,records)),
        'max_record_bytes':maximum, 'pack_index_bytes':len(index),
        'sqlite_builder_version':sqlite3.sqlite_version, 'sqlite_page_bytes':4096,
        'traces':info,
        'artifacts':{p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
                     for p in sorted(OUT.iterdir()) if p.suffix in ('.DB','.PAK','.BIN')},
        'limits':'IDs are positions within this pinned manifest, not stable save-game IDs. '
                 'Raw source blocks retain comments and original text; no typed object loading, '
                 'world references, compression, writes or save crash recovery are tested.'}
    (OUT/'corpus.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('source_files','artifacts')},indent=2))

if __name__ == '__main__':
    main()
