#!/usr/bin/env python3
"""Native sanitizer and headless DOS comparison for Sparrow approach."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/navigation'
NATIVE='endless-sky-dos-native-reference:local'
TOOLS='modern-arena-tools:0.1'
SOURCES=['dos/navigation/approach.c','dos/navigation/check.c',
         'dos/pursuit/pursuit.c','dos/motion/motion.c']
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def docker(image,args):
    return subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
       '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',image,*args],
       check=True,capture_output=True,text=True,timeout=180)
def fields(output):
    d=dict(line.split('=',1) for line in output.splitlines() if '=' in line)
    assert d.get('status')=='pass',output
    return d
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--reuse-oracle',action='store_true')
    args=parser.parse_args()
    if not args.reuse_oracle: subprocess.run(['python3',str(ROOT/'dos/navigation/native.py')],check=True)
    provenance=json.loads((WORK/'provenance.json').read_text())
    for name,key in [('oracle.cpp','oracle_source_sha256'),('native.py','runner_source_sha256')]:
        assert digest(ROOT/'dos/navigation'/name)==provenance[key]
    for name in ('commands.csv','APPROACH.DAT','PLANETS.json'):
        assert digest(WORK/name)==provenance[{'commands.csv':'trace_sha256','APPROACH.DAT':'profile_sha256',
            'PLANETS.json':'planets_sha256'}[name]]
    for name,sha in provenance['behavior_source_sha256'].items():
        assert digest(ROOT/'source'/name)==sha
    run=WORK/'run'; run.mkdir(exist_ok=True)
    shutil.copyfile(WORK/'APPROACH.DAT',run/'APPROACH.DAT')
    with (WORK/'commands.csv').open(newline='') as f: rows=list(csv.DictReader(f))
    trace=bytearray(b'ESAPTRC1'+struct.pack('<I',len(rows)))
    for row in rows:
        name=row.pop('name').encode('ascii'); assert len(name)<32
        vals=[float(v) for v in row.values()]; assert len(vals)==13
        trace+=name.ljust(32,b'\0')+struct.pack('<13d',*vals)
    (run/'TRACE.BIN').write_bytes(trace)
    flags=['-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror']
    docker(NATIVE,['gcc',*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
                   *SOURCES,'-lm','-o','.work/navigation/check-native'])
    native=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1',
          'UBSAN_OPTIONS=halt_on_error=1','.work/navigation/check-native',
          '.work/navigation/run/TRACE.BIN','.work/navigation/run/APPROACH.DAT'])
    dos_flags=['-std=gnu99','-O2','-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2',
               '-Wall','-Wextra','-Werror']
    docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dos_flags,*SOURCES,
                  '-lm','-o','.work/navigation/run/APPROACH.EXE'])
    archive=ROOT/'.work/toolchain/csdpmi7b.zip'
    assert digest(archive)=='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z: (run/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    (run/'check.conf').write_text(base+'[autoexec]\nmount c /work/.work/navigation/run\nc:\n'
          'CWSDPMI -s-\nAPPROACH.EXE TRACE.BIN APPROACH.DAT > RESULT.TXT\nexit\n')
    (run/'RESULT.TXT').unlink(missing_ok=True)
    dosbox=docker(TOOLS,['timeout','90s','xvfb-run','-a','dosbox','-conf',
                         '.work/navigation/run/check.conf'])
    (run/'dosbox.log').write_text(dosbox.stdout+dosbox.stderr)
    result={'schema':1,'oracle':provenance,'native_asan_ubsan':fields(native.stdout),
            'dosbox_16mib_20k':fields((run/'RESULT.TXT').read_text()),
            'source_sha256':{p:digest(ROOT/p) for p in SOURCES+[
                 'dos/navigation/approach.h','dos/navigation/check.py','dos/pursuit/pursuit.h',
                 'dos/motion/motion.h']},
            'trace_sha256':digest(run/'TRACE.BIN'),'profile_sha256':digest(run/'APPROACH.DAT'),
            'planets_sha256':digest(WORK/'PLANETS.json'),
            'dos_exe_sha256':digest(run/'APPROACH.EXE'),
            'tolerance':'flags exact; turn absolute <=1e-12',
            'scope':'Fully crewed stock Sparrow, stationary target, no reverse/afterburner or cruise speed.'}
    report=ROOT/'dos/reports/navigation-approach-equivalence.json'
    report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'native':result['native_asan_ubsan'],'dos':result['dosbox_16mib_20k']},indent=2))
if __name__=='__main__':
    try: main()
    except subprocess.CalledProcessError as e:
        raise SystemExit(f'{e}\n{e.stdout or ""}\n{e.stderr or ""}')
