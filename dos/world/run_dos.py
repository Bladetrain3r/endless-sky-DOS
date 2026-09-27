#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile and validate the typed world under the established DOS envelope."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
RUN = ROOT/'.work/world/run'
IMAGE = 'modern-arena-tools:0.1'
FLAGS = ['-std=gnu99', '-Os', '-march=i386', '-mtune=i586', '-mno-mmx',
         '-mno-sse', '-mno-sse2', '-Wall', '-Wextra', '-Werror']


def main():
    RUN.mkdir(parents=True, exist_ok=True)
    archive = ROOT/'.work/toolchain/csdpmi7b.zip'
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == 'deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (RUN/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    docker = ['docker', 'run', '--rm', '--network', 'none', '--user',
              f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp', '-v',
              f'{ROOT}:/work', '-w', '/work', IMAGE]
    cc = '.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc'
    subprocess.run(docker+[cc, *FLAGS, 'dos/world/read.c', '-o',
                           '.work/world/run/WORLD.EXE'], check=True)
    # Corruptions already qualified independently by native sanitizer tests.
    for name, src in [('BADREF.PAK', 'wrong_reference_kind.pak'), ('SHORT.PAK', 'truncated_tail.pak')]:
        (RUN/name).write_bytes((ROOT/'.work/world/tests'/src).read_bytes())
    conf = (ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    conf += '[autoexec]\nmount c /work/.work/world/run\nc:\n'
    for filename, log in [('WORLD.PAK', 'WORLD.TXT'), ('BADREF.PAK', 'BADREF.TXT'), ('SHORT.PAK', 'SHORT.TXT')]:
        (RUN/log).unlink(missing_ok=True)
        conf += f'CWSDPMI -s-\nWORLD.EXE {filename} > {log}\n'
    conf += 'exit\n'
    (RUN/'dosbox.conf').write_text(conf)
    subprocess.run(docker+['sh', '-c', 'timeout 90s xvfb-run -a dosbox '
                           '-conf .work/world/run/dosbox.conf > .work/world/run/dosbox.log 2>&1'],
                   check=True, timeout=110)
    outputs = {}
    for name in ('WORLD', 'BADREF', 'SHORT'):
        fields = dict(line.split('=', 1) for line in (RUN/(name+'.TXT')).read_text().splitlines() if '=' in line)
        assert fields['status'] == ('ok' if name == 'WORLD' else 'failed'), fields
        outputs[name] = fields
    expected = json.loads((RUN/'WORLD.json').read_text())
    fields = outputs['WORLD']
    for key, value in expected['counts'].items():
        assert int(fields[key]) == value, key
    assert fields['payload_hash_hex'].lower() == expected['payload_hash_hex']
    assert int(fields['memory_peak_bytes']) == expected['tracked_heap_bytes']
    assert not any(p.suffix.lower() == '.swp' for p in RUN.iterdir())
    report = dict(kind='typed-world-validation-not-gameplay', results=outputs,
                  pack={k:v for k,v in expected.items() if k != 'identities'},
                  image_id=subprocess.check_output(['docker', 'image', 'inspect', IMAGE,
                      '--format', '{{.Id}}'], text=True).strip(),
                  compiler_flags=FLAGS, exe_bytes=(RUN/'WORLD.EXE').stat().st_size,
                  config=conf, reader_sha256=hashlib.sha256((ROOT/'dos/world/read.c').read_bytes()).hexdigest(),
                  swap='disabled; no swap file observed',
                  memory_scope='directory+single payload malloc requests; excludes code, stdio, stack, runtime',
                  limitations=['Sequential validation and reference lookup, not decoded live world or game FPS.',
                               'FNV is accidental corruption detection, not authentication.',
                               'Free-page samples before/after are not peak memory measurements.'])
    (ROOT/'dos/reports/world-baseline.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
