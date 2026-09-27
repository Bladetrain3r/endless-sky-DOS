#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Camera invariants and full-scene raster parity under native sanitizers."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/flight'
FLAGS=['-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
       '-fno-omit-frame-pointer','-no-pie']
SOURCES=['dos/flight/camera_test.c','dos/flight/scene.c','dos/flight/sprite.c',
         'dos/flight/sprite_reference.c']
subprocess.run(['gcc',*FLAGS,'-DREFERENCE_RENDERER','-Dscene_init=reference_scene_init',
                '-Dscene_step=reference_scene_step','-Dscene_draw=reference_scene_draw',
                '-c',str(ROOT/'dos/flight/scene.c'),'-o',str(WORK/'camera-reference.o')],check=True)
subprocess.run(['gcc',*FLAGS,*(str(ROOT/s) for s in SOURCES),str(WORK/'camera-reference.o'),
                '-lm','-o',str(WORK/'camera-test')],check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
p=subprocess.run([str(WORK/'camera-test')],cwd=WORK/'run',env=env,capture_output=True,text=True,timeout=60)
assert p.returncode==0 and 'status=pass' in p.stdout and not p.stderr,(p.returncode,p.stdout,p.stderr)
r=dict(line.split('=',1) for line in p.stdout.splitlines())
r['sources']={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in SOURCES}
r['sanitizers']='ASan+UBSan; leak checking disabled'
r['invariants']='bounded continuous route returns to origin; camera never alters world ship state'
(ROOT/'dos/reports/flight-camera-tests.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r))
