#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/test the same collision queries on native and DOS; retain failures too."""
import argparse
import csv
import struct
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/collision'
SOURCES=['dos/collision/check.c','dos/collision/mask.c','dos/motion/motion.c']
NATIVE='endless-sky-dos-native-reference:local'
TOOLS='modern-arena-tools:0.1'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def docker(image,args,check=True):
    return subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
        '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',image,*args],check=check,capture_output=True,text=True,timeout=180)
def fields(s): return dict(line.split('=',1) for line in s.splitlines() if '=' in line)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--reuse-oracle',action='store_true');args=parser.parse_args()
    if not args.reuse_oracle: subprocess.run(['python3',str(ROOT/'dos/collision/native.py')],check=True)
    run=WORK/'run';run.mkdir(parents=True,exist_ok=True)
    provenance=json.loads((WORK/'provenance.json').read_text())
    assert digest(WORK/'masks.txt')==provenance['masks_sha256']
    assert digest(WORK/'tests.tsv')==provenance['tests_sha256']
    assert digest(ROOT/'dos/collision/oracle.cpp')==provenance['oracle_source_sha256']
    assert digest(ROOT/'dos/collision/native.py')==provenance['runner_source_sha256']
    tokens=iter((WORK/'masks.txt').read_text().split());assert next(tokens)=='ESMASK1'
    count=int(next(tokens));blob=bytearray(b'ESMASK1\0'+struct.pack('<I',count))
    for _ in range(count):
        id,outlines,points=[int(next(tokens)) for _ in range(3)];radius=float(next(tokens))
        blob.extend(struct.pack('<IIId',id,outlines,points,radius))
        for _ in range(outlines):
            n=int(next(tokens));blob.extend(struct.pack('<I',n))
            for _ in range(n): blob.extend(struct.pack('<dd',float(next(tokens)),float(next(tokens))))
    assert next(tokens,None) is None
    (run/'MASKS.BIN').write_bytes(blob)
    with (WORK/'tests.tsv').open() as f: rows=list(csv.DictReader(f,delimiter='\t'))
    blob=bytearray(struct.pack('<I',len(rows)))
    for r in rows:
        blob.extend(struct.pack('<II4did',int(r['mask_id']),int(r['angle_steps']),
            *[float(r[k]) for k in ('sx','sy','vx','vy')],int(r['contains']),float(r['collision_fraction'])))
    (run/'TESTS.BIN').write_bytes(blob)
    flags=['-std=gnu99','-O2','-Wall','-Wextra','-Werror']
    docker(NATIVE,['gcc',*flags,'-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',*SOURCES,'-lm','-o','.work/collision/check-native'])
    native=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',
        '.work/collision/check-native','.work/collision/run/MASKS.BIN','.work/collision/run/TESTS.BIN'],check=False)
    (WORK/'native-check.txt').write_text(native.stdout+native.stderr)
    original=(run/'MASKS.BIN').read_bytes()
    broken=[b'',original[:20],original+b'x']
    for offset,fmt,value in [(0,'8s',b'BADMAGIC'),(16,'I',17),(20,'I',1025),(24,'d',float('nan')),(32,'I',0)]:
        data=bytearray(original);struct.pack_into('<'+fmt,data,offset,value);broken.append(bytes(data))
    for data in broken:
        (run/'BAD.BIN').write_bytes(data)
        rejected=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',
            '.work/collision/check-native','.work/collision/run/BAD.BIN','.work/collision/run/TESTS.BIN'],check=False)
        assert rejected.returncode==1 and not rejected.stderr,(rejected.returncode,rejected.stderr)
    (run/'BAD.BIN').unlink()
    dos_flags=flags+['-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2']
    docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dos_flags,*SOURCES,'-lm','-o','.work/collision/run/COLLIDE.EXE'])
    archive=ROOT/'.work/toolchain/csdpmi7b.zip'
    assert digest(archive)=='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z: (run/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config=base+'[autoexec]\nmount c /work/.work/collision/run\nc:\nCWSDPMI -s-\nCOLLIDE.EXE MASKS.BIN TESTS.BIN > RESULT.TXT\nCOLLIDE.EXE MASKS.BIN TESTS.BIN precision53 > FPU53.TXT\nexit\n'
    (run/'check.conf').write_text(config);(run/'RESULT.TXT').unlink(missing_ok=True)
    out=docker(TOOLS,['timeout','150s','xvfb-run','-a','dosbox','-conf','.work/collision/run/check.conf'])
    (run/'dosbox.log').write_text(out.stdout+out.stderr)
    results={'native':fields(native.stdout),'dos':fields((run/'RESULT.TXT').read_text())}
    report={**results,'source_sha256':{p:digest(ROOT/p) for p in SOURCES+['dos/collision/mask.h','dos/collision/check.py']},
        'masks_sha256':digest(run/'MASKS.BIN'),'queries_sha256':digest(run/'TESTS.BIN'),
        'dos_config':config,'dos_flags':dos_flags,'oracle':provenance,'malformed_masks_rejected':len(broken),
        'dos_53bit_precision_diagnostic':fields((run/'FPU53.TXT').read_text()),
        'fpu_requirement':'Default DOS precision and explicit PC_53 both tested; flight runtime unchanged',
        'sanitizers':'Native ASan+UBSan; leak checking disabled','swap_files':list(str(p) for p in run.glob('*.SWP'))}
    (ROOT/'dos/reports/collision-equivalence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(results,indent=2))
    assert not native.stderr,native.stderr
    assert report['dos_53bit_precision_diagnostic'].get('status')=='pass'
    assert not any(p.suffix.lower()=='.swp' for p in run.iterdir())
    assert all(r.get('status')=='pass' for r in results.values()),'collision equivalence failed; inspect report'
if __name__=='__main__':
    try: main()
    except subprocess.CalledProcessError as e: raise SystemExit(f'{e}\n{e.stdout}\n{e.stderr}')
