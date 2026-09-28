#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/run a bounded headless DOS flight prototype and retain compact evidence."""
import argparse
import csv
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
         'dos/flight/scene.c','dos/flight/power.c','dos/propulsion/propulsion.c','dos/flight/sprite_reference.c','dos/world/active.c',
         'dos/shields/shields.c','dos/resources/resources.c','dos/damage/damage.c','dos/flight/practice.c','dos/projectile/projectile.c','dos/collision/mask.c',
         'dos/flight/pilot.c','dos/flight/input.c','dos/flight/input_state.c','dos/motion/motion.c']
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
    parser.add_argument('--resource-stress',choices=('energy','heat'),help='deliberately constrained trainer resources')
    parser.add_argument('--ships',type=int,default=6,choices=range(1,65))
    parser.add_argument('--frames',type=int,default=120)
    parser.add_argument('--headless-demo',action='store_true')
    parser.add_argument('--reference',action='store_true')
    parser.add_argument('--camera',action='store_true')
    parser.add_argument('--destructible-target',action='store_true',help='damage and destruction in the firing route')
    parser.add_argument('--moving-target',action='store_true',help='moving target in the held-fire route')
    parser.add_argument('--practice-test',action='store_true',help='stationary held-fire training route')
    parser.add_argument('--replay',action='store_true',help='replay a pilot control route through native-matched motion')
    args=parser.parse_args()
    if args.resource_stress and not args.practice_test: parser.error("resource-stress requires practice-test")
    if args.destructible_target and not args.practice_test: parser.error('destructible-target requires practice-test')
    if args.moving_target and not args.practice_test: parser.error('moving-target requires practice-test')
    if args.practice_test and (args.camera or args.replay): parser.error('practice-test is a separate controlled route')
    if args.replay and args.camera: parser.error('pilot replay follows the ship; --camera selects the scripted pan')
    if args.replay and args.frames>450: parser.error('pilot reference route is bounded to450frames/900ticks')
    if not 1<=args.frames<=3600: parser.error('frames must be1..3600')
    RUN.mkdir(parents=True,exist_ok=True)
    docker=['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
            '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',IMAGE]
    subprocess.run(docker+['python3','dos/flight/assets.py'],check=True)
    subprocess.run(docker+['python3','dos/flight/pilot_assets.py'],check=True)
    subprocess.run(docker+['python3','dos/projectile/assets.py'],check=True)
    subprocess.run(docker+['python3','dos/damage/assets.py'],check=True)
    subprocess.run(docker+['python3','dos/resources/assets.py'],check=True)
    subprocess.run(docker+['python3','dos/propulsion/assets.py'],check=True)
    subprocess.run(docker+['python3','dos/shields/assets.py'],check=True)
    shutil.copyfile(ROOT/'.work/world/run/WORLD.PAK',RUN/'WORLD.PAK')
    archive=ROOT/'.work/toolchain/csdpmi7b.zip'
    assert digest(archive)=='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (RUN/'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    inputs=SOURCES+[str(p.relative_to(ROOT)) for p in sorted((ROOT/'dos/flight').glob('*.h'))]+['dos/world/active.h','dos/motion/motion.h','dos/projectile/projectile.h','dos/collision/mask.h','dos/damage/damage.h','dos/shields/shields.h','dos/resources/resources.h','dos/propulsion/propulsion.h']
    source_hashes={s:digest(ROOT/s) for s in inputs}
    subprocess.run(docker+['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*FLAGS,
                          *(['-DREFERENCE_RENDERER'] if args.reference else []),*SOURCES,'-lm','-o','.work/flight/run/FLIGHT.EXE'],check=True)
    assert source_hashes=={s:digest(ROOT/s) for s in inputs}, 'source changed during build'
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    options=f'--ships {args.ships} --frames {args.frames}'
    if args.camera: options+=' --camera'
    if args.replay: options+=' --replay'
    if args.practice_test: options+=' --practice-test'
    if args.resource_stress: options+=' --resource-stress '+args.resource_stress
    if not args.moving_target: options+=' --stationary-target'
    if not args.destructible_target: options+=' --invulnerable-target'
    if args.headless_demo: options+=' --demo --seconds 4'
    conf=base+'[autoexec]\nmount c /work/.work/flight/run\nc:\nCWSDPMI -s-\n'
    conf+=f'FLIGHT.EXE {options} > FLIGHT.TXT\nexit\n'
    (RUN/'bench.conf').write_text(conf)
    # Local watch command uses the user's DOSBox, still with explicit reference settings.
    hostconf=base+f'[autoexec]\nmount c "{RUN}"\nc:\nCWSDPMI -s-\n'
    hostconf+='FLIGHT.EXE --demo --seconds 30 --camera > DEMO.TXT\nexit\n'
    (ROOT/'.work/flight/demo.conf').write_text(hostconf)
    (ROOT/'.work/flight/pilot.conf').write_text(hostconf.replace('--seconds 30 --camera','--seconds 120 --pilot'))
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
    if args.replay:
        ticks=int(fields['simulation_ticks'])
        assert int(fields['pilot_ticks'])==ticks
        with (ROOT/'.work/motion/native.csv').open() as f:
            expected=next(r for r in csv.DictReader(f) if r['case']=='sparrow_pilot_route' and int(r['tick'])==ticks)
        for key,offset in [('x',275.),('y',373.),('vx',0.),('vy',0.)]:
            assert abs(float(fields['pilot_'+key])-float(expected[key])-offset)<1e-7,(key,fields,expected)
        assert int(fields['pilot_angle'])==int(expected['angle_steps'])
    if args.practice_test:
        ticks=int(fields['simulation_ticks'])
        if not args.resource_stress: assert int(fields['practice_shots'])==(ticks+11)//12,fields
        elif ticks>=960:
            assert int(fields['practice_shots'])<(ticks+11)//12,fields
            assert int(fields['blocked_'+args.resource_stress+'_ticks'])>0,fields
        if not args.resource_stress and args.destructible_target and ticks>=(960 if args.moving_target else 120):
            assert fields['target_destroyed']=='1' and int(fields['practice_hits'])==7,fields
        elif not args.resource_stress and not args.destructible_target and not args.moving_target:
            assert int(fields['practice_hits'])>=max(0,(ticks-24)//12),fields
        assert int(fields['target_moving'])==int(args.moving_target),fields
        if not args.destructible_target: assert int(fields['target_ticks'])==ticks,fields
        assert fields['practice_dropped']=='0',fields
    assert not any(p.suffix.lower()=='.swp' for p in RUN.iterdir())
    assert (RUN/'FRAME.IDX').stat().st_size==480000
    subprocess.run(docker+['python3','-c',
        'import sys; sys.path.insert(0,"dos/flight"); from run import convert; convert()'],check=True)
    report=dict(reference_renderer=args.reference,kind='bounded-practice-fire' if args.practice_test else ('bounded-pilot-replay' if args.replay else 'scripted-indexed-flight-prototype-not-native-gameplay'),results=fields,
                image_id=subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',IMAGE],text=True).strip(),
                compiler_flags=FLAGS,config=conf,source_sha256=source_hashes,
                shield_profile_sha256=digest(RUN/'SHIELD.DAT'),
                propulsion_profile_sha256=digest(RUN/'PROPULSE.DAT'),
                resource_profile_sha256=digest(RUN/'RESOURCE.DAT'),
                resource_stress=args.resource_stress,
                damage_profile_sha256=digest(RUN/'DAMAGE.DAT'),
                practice_profile=json.loads((RUN/'blaster.json').read_text()),
                pilot_profile=json.loads((RUN/'pilot.json').read_text()),pack_sha256=digest(RUN/'WORLD.PAK'),frame_sha256=digest(RUN/'FRAME.IDX'),
                exe_bytes=(RUN/'FLIGHT.EXE').stat().st_size,assets=json.loads((RUN/'assets.json').read_text()),
                scope='Explicit residency counts world store+decoded system, frame, sprites, blend tables. Excludes runtime/code/stack/stdio and VRAM.',
                limits=['Background traffic is scripted; pilot replay uses stock-Sparrow motion. Shared propulsion/gun energy/heat; native overheat drift; reduced target health, optional invulnerability, player shield recharge; no AI/target regen/status effects.',
                        'Sol Earth/Luna fixed epoch0; scene placement selected for inspection.',
                        'Benchmark advances2simticks per render; demo60Hz accumulator/30Hz render target.',
                        'No faction recoloring or native animation interpolation yet.'])
    stem='flight-demo' if args.headless_demo else f'flight-{args.ships}-ships'
    if args.reference: stem+='-reference'
    if args.replay: stem+='-pilot'
    if args.practice_test: stem+='-practice'
    if args.moving_target: stem+='-moving'
    if args.destructible_target: stem+='-damage'
    if args.resource_stress: stem+='-'+args.resource_stress
    if args.camera: stem+='-camera'
    else: stem+='-profile'
    shutil.copyfile(RUN/'preview.png',ROOT/'.work/flight'/f'{stem}.png')
    shutil.copyfile(RUN/'FRAME.IDX',ROOT/'.work/flight'/f'{stem}.idx')
    (ROOT/'dos/reports'/f'{stem}.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(fields,indent=2))


if __name__=='__main__': main()
