#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native reader contracts and format tests; run inside the native tools image."""
import copy
import json
import math
import os
from pathlib import Path
import struct
import subprocess

from pack import build, fnv, identity

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/world/tests'


def fixture():
    system = dict(name='First', display='First "star"', valid=True, hidden=False,
                  shrouded=False, inaccessible=False, x=-1.5, y=2.25, jump_range=0.,
                  arrival_hyper=0., arrival_jump=0., departure_hyper=1000.,
                  departure_jump=1000., habitable=123.125, attributes=['alpha', 'uninhabited'],
                  links=['Second'], objects=[dict(index=0, parent=-1, planet=None,
                  sprite='star/g', distance=0., period=0., offset=0.),
                  dict(index=1, parent=0, planet='Shared', sprite=None,
                       distance=100., period=25., offset=-0.5)],
                  trade=[dict(commodity='Food', price=0)])
    second = dict(system, name='Second', display='Second', links=['First'], objects=[], trade=[])
    return dict(schema=1, systems=[system, second], planets=[dict(name='Shared', display='Shäréd',
                valid=True, inhabited=False, attributes=[], systems=['First', 'Second'])],
                commodities=[dict(name='Food', ordinal=0, low=100, high=600)])


def run(exe, path, good):
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    p = subprocess.run([str(exe), str(path)], text=True, capture_output=True, env=env, timeout=45)
    text = p.stdout + p.stderr
    if 'AddressSanitizer' in text or 'runtime error:' in text:
        raise AssertionError(text)
    if good != (p.returncode == 0) or ('status=ok' in text) != good:
        raise AssertionError(f'{path.name}: exit={p.returncode}\n{text}')
    return dict(line.split('=', 1) for line in p.stdout.splitlines() if '=' in line)


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    exe = WORK / 'world-reader'
    subprocess.run(['gcc', '-std=gnu99', '-O1', '-g', '-fno-omit-frame-pointer',
                    '-fsanitize=address,undefined', '-no-pie', '-Wall', '-Wextra', '-Werror',
                    str(ROOT/'dos/world/read.c'), '-lm', '-o', str(exe)], check=True)
    original = fixture()
    path = WORK / 'GOOD.PAK'
    report = build(original, path)
    got = run(exe, path, True)
    assert int(got['records']) == 4
    assert got['payload_hash_hex'].lower() == report['payload_hash_hex']
    # Table ordering changes do not change output or identity.
    shuffled = copy.deepcopy(original)
    shuffled['systems'].reverse()
    assert build(shuffled, WORK/'ORDER.PAK')['sha256'] == report['sha256']
    assert identity(1, 'First') != identity(2, 'First')
    assert identity(1, 'First') != identity(1, 'first')
    negative_builds = 0
    for change in ('duplicate', 'missing', 'parent', 'nonfinite', 'oversize'):
        bad = copy.deepcopy(original)
        if change == 'duplicate':
            bad['systems'].append(bad['systems'][0])
        elif change == 'missing':
            bad['systems'][0]['links'] = ['Absent']
        elif change == 'parent':
            bad['systems'][0]['objects'][0]['parent'] = 0
        elif change == 'nonfinite':
            bad['systems'][0]['x'] = math.inf
        else:
            bad['systems'][0]['display'] = 'x' * 40000
        try:
            build(bad, WORK/'REJECT.PAK')
        except (ValueError, KeyError):
            negative_builds += 1
        else:
            raise AssertionError('builder accepted ' + change)
    data = path.read_bytes()
    cases = {'truncated_header': data[:12], 'truncated_tail': data[:-1], 'extra_tail': data+b'X'}
    def mutate(name, at, fmt, value):
        b = bytearray(data)
        struct.pack_into('<'+fmt, b, at, value)
        cases[name] = bytes(b)
    mutate('bad_count', 8, 'I', 0xffffffff)
    mutate('bad_max', 12, 'I', 32769)
    mutate('bad_data_offset', 16, 'I', 0)
    mutate('null_id', 24, 'Q', 0)
    mutate('duplicate_id', 48, 'Q', struct.unpack_from('<Q', data, 24)[0])
    mutate('bad_kind', 32, 'I', 99)
    mutate('bad_offset', 36, 'I', 0xfffffff0)
    mutate('bad_length', 40, 'I', 0xffffffff)
    first_offset = struct.unpack_from('<I', data, 36)[0]
    mutate('bad_payload_hash', first_offset+4, 'B', data[first_offset+4] ^ 32)
    # Replace a planet's system reference by a valid commodity ID, repairing
    # its payload checksum: type checking must reject it independently of CRC.
    for i in range(4):
        entry = 24+i*24
        ident, kind, off, n, _ = struct.unpack_from('<QIIII', data, entry)
        if kind != 2:
            continue
        b = bytearray(data)
        struct.pack_into('<Q', b, off+n-8, identity(3, 'Food'))
        struct.pack_into('<I', b, entry+20, fnv(b[off:off+n]))
        cases['wrong_reference_kind'] = bytes(b)
    for name, payload in cases.items():
        target = WORK/(name+'.pak')
        target.write_bytes(payload)
        run(exe, target, False)
    vanilla = ROOT/'.work/world/run/WORLD.PAK'
    if vanilla.exists():
        expected = json.loads(vanilla.with_suffix('.json').read_text())
        actual = run(exe, vanilla, True)
        for key, value in expected['counts'].items():
            assert int(actual[key]) == value, (key, actual[key], value)
        assert actual['payload_hash_hex'].lower() == expected['payload_hash_hex']
        assert int(actual['memory_peak_bytes']) == expected['tracked_heap_bytes']
    summary = dict(status='pass', sanitizer='ASan+UBSan; leak detection disabled',
                   builder_rejections=negative_builds, reader_corruptions_rejected=len(cases),
                   fixture_records=4, vanilla_checked=vanilla.exists(), stable_order=True)
    (WORK/'report.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
