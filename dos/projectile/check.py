#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare original Projectile traces with the DOS bolt kernel + trainer gates."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile
from assets import stage
import importlib.util
spec=importlib.util.spec_from_file_location("damage_assets",Path(__file__).resolve().parents[1]/"damage/assets.py")
damage_assets=importlib.util.module_from_spec(spec);spec.loader.exec_module(damage_assets)
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/projectile'
SOURCES=['dos/projectile/check.c','dos/projectile/projectile.c','dos/flight/practice.c',
         'dos/collision/mask.c','dos/motion/motion.c','dos/damage/damage.c']
NATIVE='endless-sky-dos-native-reference:local'
TOOLS='modern-arena-tools:0.1'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def docker(image,args,check=True):
    return subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
        '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',image,*args],check=check,capture_output=True,text=True,timeout=180)
def fields(s): return dict(line.split('=',1) for line in s.splitlines() if '=' in line)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--reuse-oracle',action='store_true');args=parser.parse_args()
    if not args.reuse_oracle: subprocess.run(['python3',str(ROOT/'dos/projectile/native.py')],check=True)
    run=WORK/'run';stage(run);damage_assets.stage(run)
    provenance=json.loads((WORK/'native-provenance.json').read_text())
    with (WORK/'native.csv').open() as stream: rows=list(csv.DictReader(stream))
    w=provenance['weapon'];blob=bytearray(struct.pack('<IdI',len(rows),w['velocity'],int(w['lifetime'])))
    ids={r['case']:i for i,r in enumerate(rows) if r['frame']=='0'}
    for r in rows:
        blob.extend(struct.pack('<6I8d',ids[r['case']],int(r['frame']),int(r['launch_angle_steps']),
            int(r['angle_steps']),int(r['is_dead']),int(r['is_removed']),
            *[float(r[k]) for k in ('launch_x','launch_y','parent_vx','parent_vy','x','y','vx','vy')]))
    (run/'TRACE.BIN').write_bytes(blob)
    flags=['-std=gnu99','-O2','-Wall','-Wextra','-Werror']
    docker(NATIVE,['gcc',*flags,'-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',*SOURCES,'-lm','-o','.work/projectile/check-native'])
    native=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',
        '.work/projectile/check-native',*['.work/projectile/run/'+p for p in ('TRACE.BIN','BLASTER.DAT','MASKS.BIN','DAMAGE.DAT')]],check=False)
    (WORK/'native-check.txt').write_text(native.stdout+native.stderr)
    assert native.returncode==0 and not native.stderr,(native.stdout,native.stderr)
    dos_flags=flags+['-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2']
    docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dos_flags,*SOURCES,'-lm','-o','.work/projectile/run/BOLTCHEK.EXE'])
    archive=ROOT/'.work/toolchain/csdpmi7b.zip'
    assert digest(archive)=='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z: (run/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config=base+'[autoexec]\nmount c /work/.work/projectile/run\nc:\nCWSDPMI -s-\nBOLTCHEK.EXE TRACE.BIN BLASTER.DAT MASKS.BIN DAMAGE.DAT > RESULT.TXT\nexit\n'
    (run/'check.conf').write_text(config);(run/'RESULT.TXT').unlink(missing_ok=True)
    out=docker(TOOLS,['timeout','60s','xvfb-run','-a','dosbox','-conf','.work/projectile/run/check.conf'])
    (run/'dosbox.log').write_text(out.stdout+out.stderr)
    results={'native':fields(native.stdout),'dos':fields((run/'RESULT.TXT').read_text())}
    report={**results,'oracle':provenance,'source_sha256':{p:digest(ROOT/p) for p in SOURCES+['dos/projectile/projectile.h','dos/flight/practice.h','dos/damage/damage.h','dos/projectile/check.py','dos/projectile/assets.py']},
        'trace_sha256':digest(run/'TRACE.BIN'),'profile':json.loads((run/'blaster.json').read_text()),
        'dos_flags':dos_flags,'dos_config':config,'sanitizers':'ASan+UBSan; no leak claim',
        'tolerance':'Position/velocity <=1e-10; exact heading, dead/removal flags through native frame48.',
        'scope':'Native constructor/Move parity; composition tests for swept nearest-hit, zero/start-inside/miss, expiry, trainer cadence/reset and bounded pool. Not full Engine collision/faction/damage equivalence.'}
    (ROOT/'dos/reports/projectile-equivalence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(results,indent=2))
    assert all(r.get('status')=='pass' for r in results.values()),'projectile check failed'
    assert not any(p.suffix.lower()=='.swp' for p in run.iterdir())
if __name__=='__main__':
    try: main()
    except subprocess.CalledProcessError as e: raise SystemExit(f'{e}\n{e.stdout}\n{e.stderr}')
