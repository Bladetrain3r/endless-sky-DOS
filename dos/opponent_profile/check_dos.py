#!/usr/bin/env python3
"""Build and run the 430-row Star Barge generic-kernel checker in DOSBox."""
# SPDX-License-Identifier: GPL-3.0-or-later
import csv
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile
from assets import stage

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/opponent_profile'
TOOLS = 'modern-arena-tools:0.1'
CASES = ('healthy', 'shield_drain', 'energy_starved', 'overheat_recovery', 'hull_disabled')

def docker(args,image=TOOLS):
    result = subprocess.run(['docker', 'run', '--rm', '--network', 'none', '--user',
        f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp', '-v', f'{ROOT}:/work',
        '-w', '/work', image, *args], capture_output=True, text=True, timeout=180)
    if result.returncode: raise RuntimeError(f'Docker command failed: {result.stdout}\n{result.stderr}')
    return result.stdout + result.stderr

def main():
    run = WORK / 'run'; run.mkdir(parents=True, exist_ok=True)
    stage(run)
    stage(WORK / 'staged')
    docker(['python3','dos/opponent_profile/check.py'],'endless-sky-dos-native-reference:local')
    with (WORK / 'native.csv').open(newline='') as f: rows = list(csv.DictReader(f))
    assert len(rows) == 430
    trace = bytearray(struct.pack('<I', len(rows)))
    for row in rows:
        trace.extend(struct.pack('<3I3d2IdI', CASES.index(row['case']), int(row['tick']),
            int(row['prior_disabled']), float(row['shields']), float(row['energy']),
            float(row['heat']), int(row['overheated']), int(row['delay']),
            float(row['hull']), int(row['disabled'])))
    (run / 'TRACE.BIN').write_bytes(trace)
    source = ('dos/opponent_profile/check.c', 'dos/resources/resources.c', 'dos/shields/shields.c')
    flags = ['-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror', '-march=i386',
             '-mtune=i586', '-mno-mmx', '-mno-sse', '-mno-sse2']
    docker(['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc', *flags, *source,
            '-lm', '-o', '.work/opponent_profile/run/CHECK.EXE'])
    archive = ROOT / '.work/toolchain/csdpmi7b.zip'
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == 'deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (run / 'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base = (ROOT / 'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config = (base + '[autoexec]\nmount c /work/.work/opponent_profile/run\nc:\nCWSDPMI -s-\n'
              'CHECK.EXE TRACE.BIN BARGERES.DAT BARGESH.DAT BARGEHP.DAT > RESULT.TXT\nexit\n')
    (run / 'check.conf').write_text(config)
    (run / 'RESULT.TXT').unlink(missing_ok=True)
    (run / 'dosbox.log').write_text(docker(['timeout', '60s', 'xvfb-run', '-a', 'dosbox',
                                            '-conf', '.work/opponent_profile/run/check.conf']))
    result = (run / 'RESULT.TXT').read_text().strip()
    if 'status=pass rows=430 max_error=0' not in result:
        raise RuntimeError(f'DOS comparison failed: {result}')
    report = {'rows': len(rows), 'dos_result': result,
              'trace_sha256': hashlib.sha256(trace).hexdigest(),
              'dos_flags': flags, 'config': config}
    (WORK / 'dos-check.json').write_text(json.dumps(report, indent=2) + '\n')
    inputs = list((ROOT / 'dos/opponent_profile').glob('*.py')) + list((ROOT / 'dos/opponent_profile').glob('*.c')) + [ROOT / 'dos/opponent_profile/oracle.cpp']
    inputs += [ROOT / ('dos/' + name) for name in ('resources/resources.c','resources/resources.h','shields/shields.c','shields/shields.h')]
    evidence = {'native': json.loads((WORK / 'check.json').read_text()), 'dos': report,
                'provenance': json.loads((WORK / 'provenance.json').read_text()),
                'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
                'profile_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in run.glob('BARGE*.DAT')},
                'scope': 'Native Ship::DoGeneration vs shared resource/shield kernels; 5 fitted Star Barge cases. Flight integration and sanitizer tests are separate.'}
    (ROOT / 'dos/reports/opponent-profile-equivalence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(result)

if __name__ == '__main__': main()
