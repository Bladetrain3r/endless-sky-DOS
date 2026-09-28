#!/usr/bin/env python3
"""Compare bounded AI pursuit helpers to actual upstream methods on native and DOS."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import zipfile
from assets import stage

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/pursuit'
NATIVE='endless-sky-dos-native-reference:local'
TOOLS='modern-arena-tools:0.1'
SOURCES=['dos/pursuit/pursuit.c','dos/pursuit/check.c','dos/motion/motion.c']
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def docker(image,args):
    return subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
        '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',image,*args],
        check=True,capture_output=True,text=True,timeout=180)
def fields(s):
    result=dict(line.split('=',1) for line in s.splitlines() if '=' in line)
    assert result.get('status')=='pass',s
    return result
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--reuse-oracle',action='store_true')
    args=parser.parse_args()
    if not args.reuse_oracle: subprocess.run(['python3',str(ROOT/'dos/pursuit/native.py')],check=True)
    run=WORK/'run'; run.mkdir(parents=True,exist_ok=True)
    stage(run)
    provenance=json.loads((WORK/'provenance.json').read_text())
    with (WORK/'commands.csv').open(newline='') as f: commands=list(csv.DictReader(f))
    with (WORK/'intercepts.csv').open(newline='') as f: intercepts=list(csv.DictReader(f))
    assert len(commands)==provenance['commands'] and len(intercepts)==provenance['intercepts']
    trace=bytearray(b'ESPTRACE'+struct.pack('<2I',len(commands),len(intercepts)))
    for row in commands:
        name=row.pop('name').encode('ascii'); assert len(name)<32
        values=[float(x) for x in row.values()]; assert len(values)==13
        trace+=name.ljust(32,b'\0')+struct.pack('<13d',*values)
    for row in intercepts:
        name=row.pop('name').encode('ascii'); assert len(name)<32
        values=[float(x) for x in row.values()]; assert len(values)==6
        trace+=name.ljust(32,b'\0')+struct.pack('<6d',*values)
    (run/'TRACE.BIN').write_bytes(trace)
    flags=['-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror']
    docker(NATIVE,['gcc',*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
                   *SOURCES,'-lm','-o','.work/pursuit/check-native'])
    native=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',
                          '.work/pursuit/check-native','.work/pursuit/run/TRACE.BIN','.work/pursuit/run/PURSUIT.DAT',
                          '.work/pursuit/run/BAD.DAT'])
    assert not native.stderr,native.stderr
    dos_flags=['-std=gnu99','-O2','-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2',
               '-Wall','-Wextra','-Werror']
    docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dos_flags,
                  *SOURCES,'-lm','-o','.work/pursuit/run/PURSUIT.EXE'])
    archive=ROOT/'.work/toolchain/csdpmi7b.zip'
    assert digest(archive)=='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z: (run/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config=base+'[autoexec]\nmount c /work/.work/pursuit/run\nc:\nCWSDPMI -s-\nPURSUIT.EXE TRACE.BIN PURSUIT.DAT BAD.DAT > RESULT.TXT\nexit\n'
    (run/'check.conf').write_text(config); (run/'RESULT.TXT').unlink(missing_ok=True)
    output=docker(TOOLS,['timeout','90s','xvfb-run','-a','dosbox','-conf','.work/pursuit/run/check.conf'])
    (run/'dosbox.log').write_text(output.stdout+output.stderr)
    results={'native_asan_ubsan':fields(native.stdout),'dosbox_16mib_20k':fields((run/'RESULT.TXT').read_text())}
    report={'schema':1,'oracle':provenance,'results':results,'source_sha256':
            {p:digest(ROOT/p) for p in SOURCES+['dos/pursuit/pursuit.h','dos/pursuit/check.py',
            'dos/pursuit/assets.py','dos/motion/motion.h']},'profile_sha256':digest(run/'PURSUIT.DAT'),
            'trace_sha256':digest(run/'TRACE.BIN'),'dos_exe_sha256':digest(run/'PURSUIT.EXE'),
            'dos_flags':dos_flags,'profile_format':'ESPURS1 NUL + five LE binary64 values',
            'tolerance':'Turn <=1e-12 absolute, finite intercept <=1e-8 absolute; command flags and NaN/infinity classifications exact.',
            'scope':'Stock loaded Star Barge AI::MoveToAttack, TurnToward default precision, and RendezvousTime only. No target selection, tactics, fire, retreat, avoidance, factions, or full AI.',
            'sanitizers':'ASan+UBSan for C kernel; native game objects unchanged and unsanitized.'}
    (ROOT/'dos/reports/pursuit-equivalence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(results,indent=2))
if __name__=='__main__':
    try: main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(f'{error}\n{error.stdout or ""}\n{error.stderr or ""}')
