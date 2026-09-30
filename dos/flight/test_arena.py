#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native sanitizer and 16 MiB DOSBox integration checks for the two-opponent arena."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/arena/run'
SOURCES=['dos/flight/arena.c','dos/propulsion/propulsion.c','dos/flight/target_power.c','dos/pursuit/pursuit.c','dos/flight/arena_test.c','dos/flight/opponent.c','dos/flight/threat.c','dos/flight/practice.c',
         'dos/flight/pilot.c','dos/shields/shields.c','dos/resources/resources.c',
         'dos/motion/motion.c','dos/projectile/projectile.c','dos/collision/mask.c',
         'dos/damage/damage.c']
HEADERS=['dos/flight/arena.h','dos/flight/input_state.h','dos/propulsion/propulsion.h','dos/flight/target_power.h','dos/flight/opponent.h','dos/pursuit/pursuit.h','dos/flight/threat.h','dos/flight/practice.h','dos/flight/pilot.h',
         'dos/shields/shields.h','dos/resources/resources.h','dos/motion/motion.h',
         'dos/projectile/projectile.h','dos/collision/mask.h','dos/damage/damage.h']
PROFILES=['RESOURCE.DAT','PILOT.DAT','BLASTER.DAT','DAMAGE.DAT','MASKS.BIN','SHIELD.DAT',
          'BARGERES.DAT','BARGESH.DAT','BARGEPRO.DAT','BARGEHP.DAT','PLAYER.DAT','PURSUIT.DAT','CWSDPMI.EXE']
NATIVE='endless-sky-dos-native-reference:local'
TOOLS='modern-arena-tools:0.1'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def docker(image,args,cwd='/work'):
    r=subprocess.run(['docker','run','--rm','--network','none','--user',
        f'{os.getuid()}:{os.getgid()}','-e','HOME=/tmp','-v',f'{ROOT}:/work',
        '-w',cwd,image,*args],capture_output=True,text=True,timeout=120)
    if r.returncode: raise RuntimeError(r.stdout+r.stderr)
    return r
RUN.mkdir(parents=True,exist_ok=True)
for name in PROFILES:
    shutil.copyfile(ROOT/'.work/flight/run'/name,RUN/name)
# Failed load must leave an existing state intact.
(RUN/'BAD.DAT').write_bytes(b'ESPLAYER1'+b'\0'*16)
flags=['-std=gnu99','-O2','-Wall','-Wextra','-Werror']
docker(NATIVE,['gcc',*flags,'-g','-fsanitize=address,undefined',
    '-fno-omit-frame-pointer','-no-pie',*SOURCES,'-lm',
    '-o','.work/flight/arena/run/check-native'])
native=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1',
    'UBSAN_OPTIONS=halt_on_error=1','./check-native'],
    '/work/.work/flight/arena/run')
assert not native.stderr,native.stderr
dosflags=flags+['-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2']
docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dosflags,
    *SOURCES,'-lm','-o','.work/flight/arena/run/ARENA.EXE'])
base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
conf=base+'[autoexec]\nmount c /work/.work/flight/arena/run\nc:\nCWSDPMI -s-\nARENA.EXE > RESULT.TXT\nexit\n'
(RUN/'check.conf').write_text(conf)
(RUN/'RESULT.TXT').unlink(missing_ok=True)
docker(TOOLS,['timeout','60s','xvfb-run','-a','dosbox',
    '-conf','.work/flight/arena/run/check.conf'])
fields=lambda s:dict(line.split('=',1) for line in s.splitlines() if '=' in line)
results={'native':fields(native.stdout),'dos':fields((RUN/'RESULT.TXT').read_text())}
assert results['native']==results['dos'],results
assert results['native'].get('status')=='pass',results
report={'results':results,
    'source_sha256':{p:digest(ROOT/p) for p in SOURCES+HEADERS+['dos/flight/test_arena.py']},
    'profile_sha256':{p:digest(RUN/p) for p in PROFILES},
    'dos_flags':dosflags,'dos_config':conf,
    'scope':'Two independently budgeted pursuit enemies, nearest projectile collision independent of selection, same-tick kill/disable, source-death bolts sharing player health, target cycle/nearest/reset and bounded per-owner pools. Arena layout and targeting policy are trainer features.',
    'native_hull_evidence':'Stock Sparrow data/human/ships.txt hull 300; source/Ship.cpp CacheAttributes floors default threshold to 134. PLAYER.DAT is exported by the native Ship getter oracle.',
    'sanitizers':'Portable native ASan/UBSan; leak checking disabled'}
(ROOT/'dos/reports/flight-arena-tests.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(results,indent=2))
