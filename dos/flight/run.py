#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/run a bounded headless DOS flight prototype and retain compact evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/run'
IMAGE='modern-arena-tools:0.1'
SOURCES=['dos/flight/main.c','dos/flight/video.c','dos/flight/sprite.c',
         'dos/flight/scene.c','dos/flight/sprite_reference.c','dos/world/active.c']
FLAGS=['-std=gnu99','-O2','-march=i386','-mtune=i586','-mno-mmx','-mno-sse',
       '-mno-sse2','-Wall','-Wextra','-Werror']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def convert():
    from PIL import Image
    raw=(RUN/'FLIGHT.PAL').read_bytes()
    frame=Image.frombytes('P',(800,600),(RUN/'FRAME.IDX').read_bytes())
    frame.putpalette([round(v*255/63) for v in raw])
    frame.save(RUN/'preview.png')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--ships',type=int,default=6,choices=range(1,65))
    parser.add_argument('--frames',type=int,default=120)
    parser.add_argument('--headless-demo',action='store_true')
    parser.add_argument('--reference',action='store_true')
    parser.add_argument('--camera',action='store_true')
    args=parser.parse_args()
    if not 1<=args.frames<=3600: parser.error('frames must be1..3600')
    RUN.mkdir(parents=True,exist_ok=True)
    docker=['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
            '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',IMAGE]
    subprocess.run(docker+['python3','dos/flight/assets.py'],check=True)
    shutil.copyfile(ROOT/'.work/world/run/WORLD.PAK',RUN/'WORLD.PAK')
    archive=ROOT/'.work/toolchain/csdpmi7b.zip'
    assert digest(archive)=='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (RUN/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    inputs=SOURCES+[str(p.relative_to(ROOT)) for p in sorted((ROOT/'dos/flight').glob('*.h'))]+['dos/world/active.h']
    source_hashes={s:digest(ROOT/s) for s in inputs}
    subprocess.run(docker+['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*FLAGS,
                          *(['-DREFERENCE_RENDERER'] if args.reference else []),*SOURCES,'-lm','-o','.work/flight/run/FLIGHT.EXE'],check=True)
    assert source_hashes=={s:digest(ROOT/s) for s in inputs}, 'source changed during build'
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    options=f'--ships {args.ships} --frames {args.frames}'
    if args.camera: options+=' --camera'
    if args.headless_demo: options+=' --demo --seconds 4'
    conf=base+'[autoexec]\nmount c /work/.work/flight/run\nc:\nCWSDPMI -s-\n'
    conf+=f'FLIGHT.EXE {options} > FLIGHT.TXT\nexit\n'
    (RUN/'bench.conf').write_text(conf)
    # Local watch command uses the user's DOSBox, still with explicit reference settings.
    hostconf=base+f'[autoexec]\nmount c "{RUN}"\nc:\nCWSDPMI -s-\n'
    hostconf+='FLIGHT.EXE --demo --seconds 30 --camera > DEMO.TXT\nexit\n'
    (ROOT/'.work/flight/demo.conf').write_text(hostconf)
    for filename in ('FLIGHT.TXT','FRAME.IDX'):
        (RUN/filename).unlink(missing_ok=True)
    subprocess.run(docker+['sh','-c','timeout 120s xvfb-run -a dosbox '
                           '-conf .work/flight/run/bench.conf > .work/flight/run/dosbox.log 2>&1'],
                   check=True,timeout=140)
    fields=dict(line.split('=',1) for line in (RUN/'FLIGHT.TXT').read_text().splitlines() if '=' in line)
    assert fields['status']=='ok' and fields['video_verify']=='pass',fields
    if not args.headless_demo:
        assert int(fields['frames'])==args.frames and int(fields['simulation_ticks'])==args.frames*2
    if args.headless_demo:
        assert float(fields['discarded_sim_ms'])==0,fields
        expected=float(fields['wall_elapsed_ms'])*60/1000
        allowance=float(fields['frame_max_ms'])*60/1000+3
        assert abs(int(fields['simulation_ticks'])-expected)<=allowance,fields
    assert not any(p.suffix.lower()=='.swp' for p in RUN.iterdir())
    assert (RUN/'FRAME.IDX').stat().st_size==480000
    subprocess.run(docker+['python3','-c',
        'import sys; sys.path.insert(0,"dos/flight"); from run import convert; convert()'],check=True)
    report=dict(reference_renderer=args.reference,kind='scripted-indexed-flight-prototype-not-native-gameplay',results=fields,
                image_id=subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',IMAGE],text=True).strip(),
                compiler_flags=FLAGS,config=conf,source_sha256=source_hashes,
                pack_sha256=digest(RUN/'WORLD.PAK'),frame_sha256=digest(RUN/'FRAME.IDX'),
                exe_bytes=(RUN/'FLIGHT.EXE').stat().st_size,assets=json.loads((RUN/'assets.json').read_text()),
                scope='Explicit residency counts world store+decoded system, frame, sprites, blend tables. Excludes runtime/code/stack/stdio and VRAM.',
                limits=['Scripted integer traffic; no native AI, collision, combat, UI or ship physics.',
                        'Sol Earth/Luna fixed epoch0; scene placement selected for inspection.',
                        'Benchmark advances2simticks per render; demo60Hz accumulator/30Hz render target.',
                        'No faction recoloring or native animation interpolation yet.'])
    stem='flight-demo' if args.headless_demo else f'flight-{args.ships}-ships'
    if args.reference: stem+='-reference'
    if args.camera: stem+='-camera'
    else: stem+='-profile'
    shutil.copyfile(RUN/'preview.png',ROOT/'.work/flight'/f'{stem}.png')
    shutil.copyfile(RUN/'FRAME.IDX',ROOT/'.work/flight'/f'{stem}.idx')
    (ROOT/'dos/reports'/f'{stem}.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(fields,indent=2))


if __name__=='__main__': main()
