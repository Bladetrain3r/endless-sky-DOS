#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native sanitizer and DOS composition checks on the staged flight profiles."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/power/run'
SOURCES=['dos/flight/target_power.c','dos/flight/power_test.c','dos/flight/power.c','dos/flight/pilot.c',
         'dos/flight/practice.c','dos/propulsion/propulsion.c','dos/shields/shields.c','dos/resources/resources.c',
         'dos/motion/motion.c','dos/projectile/projectile.c','dos/collision/mask.c','dos/damage/damage.c']
HEADERS=['dos/flight/target_power.h','dos/flight/power.h','dos/flight/pilot.h','dos/flight/practice.h','dos/propulsion/propulsion.h',
         'dos/shields/shields.h','dos/resources/resources.h','dos/motion/motion.h','dos/projectile/projectile.h',
         'dos/collision/mask.h','dos/damage/damage.h']
NATIVE='endless-sky-dos-native-reference:local';TOOLS='modern-arena-tools:0.1'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def docker(image,args,cwd='/work'):
    r=subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
        '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w',cwd,image,*args],capture_output=True,text=True,timeout=120)
    if r.returncode: raise RuntimeError(r.stdout+r.stderr)
    return r
RUN.mkdir(parents=True,exist_ok=True)
profiles=['PILOT.DAT','PROPULSE.DAT','RESOURCE.DAT','BLASTER.DAT','MASKS.BIN','CWSDPMI.EXE','SHIELD.DAT']
for name in profiles: shutil.copyfile(ROOT/'.work/flight/run'/name,RUN/name)
flags=['-std=gnu99','-O2','-Wall','-Wextra','-Werror']
docker(NATIVE,['gcc',*flags,'-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
    *SOURCES,'-lm','-o','.work/flight/power/run/check-native'])
native=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',
    './check-native'],'/work/.work/flight/power/run')
assert not native.stderr,native.stderr
dosflags=flags+['-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2']
docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dosflags,*SOURCES,'-lm','-o','.work/flight/power/run/POWER.EXE'])
base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
conf=base+'[autoexec]\nmount c /work/.work/flight/power/run\nc:\nCWSDPMI -s-\nPOWER.EXE > RESULT.TXT\nexit\n'
(RUN/'check.conf').write_text(conf);(RUN/'RESULT.TXT').unlink(missing_ok=True)
docker(TOOLS,['timeout','60s','xvfb-run','-a','dosbox','-conf','.work/flight/power/run/check.conf'])
fields=lambda s:dict(line.split('=',1) for line in s.splitlines() if '=' in line)
results={'native':fields(native.stdout),'dos':fields((RUN/'RESULT.TXT').read_text())}
assert all(r.get('status')=='pass' for r in results.values()),results
report={'results':results,'source_sha256':{p:digest(ROOT/p) for p in SOURCES+HEADERS+['dos/flight/test_power.py']},
    'profile_sha256':{p:digest(RUN/p) for p in profiles},'dos_config':conf,'dos_flags':dosflags,
    'scope':'App composition: shield repair from prior energy -> generation -> powered pilot/camera -> weapon, partial steering priority, heat drift/recovery, reset and trainer drain. Native Ship kernel comparison is separate propulsion-equivalence.json.',
    'sanitizers':'Portable native ASan/UBSan; leak checking disabled'}
(ROOT/'dos/reports/flight-power-tests.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(results,indent=2))
