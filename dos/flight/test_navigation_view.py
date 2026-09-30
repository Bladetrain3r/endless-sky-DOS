#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual navigation renderer/font/sprites under native ASan/UBSan, inside Docker."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/flight/navigation-view'
SOURCES=['dos/flight/navigation_view_test.c','dos/flight/navigation_view.c','dos/flight/navigation.c','dos/flight/power.c','dos/navigation/approach.c',
    'dos/flight/sprite.c','dos/flight/sprite_reference.c','dos/flight/practice.c',
    'dos/flight/target_power.c','dos/flight/pilot.c','dos/flight/opponent.c',
    'dos/flight/threat.c','dos/pursuit/pursuit.c','dos/propulsion/propulsion.c',
    'dos/motion/motion.c','dos/projectile/projectile.c','dos/collision/mask.c',
    'dos/shields/shields.c','dos/resources/resources.c','dos/damage/damage.c']
WORK.mkdir(parents=True,exist_ok=True)
# Compile the production font verbatim while excluding DOS-only VBE/BIOS calls.
font=(ROOT/'dos/flight/video.c').read_text().split('// Original compact 5x7 block font.',1)[1]
(WORK/'font.c').write_text('#include "video.h"\n// Original compact 5x7 block font.'+font)
docker=['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
    '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',
    'endless-sky-dos-native-reference:local']
flags=['-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
    '-fno-omit-frame-pointer','-no-pie']
subprocess.run(docker+['gcc',*flags,'-Idos/flight',*SOURCES,
    '.work/flight/navigation-view/font.c','-lm','-o','.work/flight/navigation-view/check'],check=True)
result=subprocess.run(docker+['sh','-c','cd .work/flight/run && '
    'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ../navigation-view/check'],
    check=True,capture_output=True,text=True,timeout=120)
assert not result.stderr,result.stderr
fields=dict(line.split('=',1) for line in result.stdout.splitlines())
assert fields['status']=='pass',fields
report={'results':fields,'sanitizers':'ASan/UBSan including leak checks',
    'scope':'Actual rasterizer and verbatim production font; edges, remote positions, selection, death, projectiles, ship scaling, dock descriptions, camera offsets; repeatability and no simulation mutations.',
    'sources':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
        for name in SOURCES+['dos/flight/video.c','dos/flight/test_navigation_view.py']}}
(ROOT/'dos/reports/flight-navigation-view-tests.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(fields,indent=2))
