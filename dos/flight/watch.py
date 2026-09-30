#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Watch the already-built prototype in the reference DOSBox container via local X11."""
import argparse
import fcntl
import hashlib
import json
import uuid
import os
from pathlib import Path
import socket
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/run'


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--campaign',action='store_true',help='persistent development pilot; autosave at ports and F5')
    parser.add_argument('--navigation',action='store_true',help='quiet Sol planet selection, landing and takeoff')
    parser.add_argument('--arena',action='store_true',help='two-opponent combat arena and cockpit HUD')
    parser.add_argument('--opponent-stock',action='store_true',help='full native Barge health instead of reduced training health')
    parser.add_argument('--opponent-stress',choices=('energy','heat'),help='constrain enemy energy or heat for lifecycle testing')
    parser.add_argument('--pursuit',action='store_true',help='native Star Barge pursuit helpers plus predictive trainer fire')
    parser.add_argument('--incoming-fire',action='store_true',help='scripted target fires back after two seconds')
    parser.add_argument('--resource-stress',choices=('energy','heat'),help='deliberate energy/heat limits for testing')
    parser.add_argument('--stationary',action='store_true',help='keep the camera fixed for comparison')
    parser.add_argument('--invulnerable-target',action='store_true',help='retain the indestructible comparison target')
    parser.add_argument('--stationary-target',action='store_true',help='retain the fixed practice target')
    parser.add_argument('--accepted',action='store_true',help='compare preserved pre-scale flight build')
    parser.add_argument('--pilot',action='store_true',help='fly the stock Sparrow with held keys')
    parser.add_argument('--seconds',type=int,default=None,help='duration1..120seconds; default30watch/120pilot')
    args=parser.parse_args()
    if args.campaign: args.navigation=True
    if args.navigation:
        args.pilot=True
        if args.arena or args.pursuit or args.incoming_fire or args.accepted or args.stationary_target or args.invulnerable_target or args.resource_stress or args.opponent_stock or args.opponent_stress:
            parser.error('navigation is a separate quiet stock-Sparrow route')
    if args.arena: args.pilot=args.pursuit=True
    run=ROOT/'.work/flight/accepted-0636c12dc' if args.accepted else RUN
    seconds=args.seconds if args.seconds is not None else (120 if args.pilot else 30)
    if not 1<=seconds<=120: parser.error('seconds must be1..120')
    if args.pilot and args.stationary: parser.error('use Tab during pilot mode to switch camera')
    if (args.opponent_stock or args.opponent_stress) and not args.pursuit: parser.error('opponent options require pursuit')
    if args.pursuit and (not args.pilot or args.accepted or args.stationary_target): parser.error('pursuit requires current pilot and moving target')
    if args.incoming_fire and (not args.pilot or args.accepted): parser.error('incoming-fire requires current --pilot build')
    if args.resource_stress and (not args.pilot or args.accepted): parser.error("resource-stress requires current --pilot build")
    if args.invulnerable_target and (not args.pilot or args.accepted): parser.error('invulnerable-target requires current --pilot build')
    if args.stationary_target and (not args.pilot or args.accepted): parser.error('stationary-target requires current --pilot build')
    camera=' --pilot' if args.pilot else ('' if args.stationary else ' --camera')
    if args.campaign: camera+=' --campaign'
    elif args.navigation: camera+=' --navigation'
    if args.arena: camera+=' --arena'
    elif args.pursuit: camera+=' --pursuit'
    if args.opponent_stock: camera+=' --opponent-stock'
    if args.opponent_stress: camera+=' --opponent-stress '+args.opponent_stress
    if args.incoming_fire: camera+=' --incoming-fire'
    if args.stationary_target: camera+=' --stationary-target'
    if args.resource_stress: camera+=' --resource-stress '+args.resource_stress
    if args.invulnerable_target: camera+=' --invulnerable-target'
    display=os.environ.get('DISPLAY','')
    if not display.startswith((':','unix:')):
        sys.exit('A local X11/XWayland DISPLAY is needed for the reference window.')
    if not (run/'FLIGHT.EXE').is_file():
        sys.exit('Build first: python3 dos/flight/run.py')
    # One launcher owns the shared run directory/config and pilot slots at a time.
    lock=(run/'watch.lock').open('a')
    try: fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError: sys.exit('A reference flight session already owns this run directory.')
    if args.campaign:
        build=json.loads((run/'BUILD.json').read_text())
        for name,key in [('FLIGHT.EXE','exe_sha256'),('SAVEKEY.json','manifest_file_sha256'),('SAVEKEY.H','header_sha256')]:
            if hashlib.sha256((run/name).read_bytes()).hexdigest()!=build[key]:
                sys.exit('Pilot build is incomplete or changed; rebuild first: '+name)
        manifest=json.loads((run/'SAVEKEY.json').read_text())
        for group,basepath in [('profiles',run),('gameplay',ROOT)]:
            for name,expected in manifest[group].items():
                if hashlib.sha256((basepath/name).read_bytes()).hexdigest()!=expected:
                    sys.exit('Persistent-pilot inputs changed; rebuild before launching: '+name)
        save=run/'SAVE';save.mkdir(exist_ok=True)
        if not any((save/n).exists() for n in ('PILOT0.SAV','PILOT1.SAV','SEED.DAT')):
            with (save/'SEED.DAT').open('xb') as f:
                f.write(uuid.uuid4().bytes+uuid.uuid4().bytes)
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    (run/'watch.conf').write_text(base+'[autoexec]\nmount c /work/.work/flight/run\nc:\n'
        f'CWSDPMI -s-\nFLIGHT.EXE --demo --seconds {seconds}{camera} > DEMO.TXT\nexit\n')
    cmd=['docker','run','--rm','--network','none','--hostname',socket.gethostname(),'--user',f'{os.getuid()}:{os.getgid()}',
         '-e','HOME=/tmp','-e',f'DISPLAY={display}',
         '--mount','type=bind,src=/tmp/.X11-unix,dst=/tmp/.X11-unix,readonly',
         '--mount',f'type=bind,src={ROOT},dst=/work,readonly',
         '--mount',f'type=bind,src={run},dst=/work/.work/flight/run', '-w','/work']
    authority=Path(os.environ.get('XAUTHORITY',str(Path.home()/'.Xauthority')))
    if authority.is_file():
        cmd+=['--mount',f'type=bind,src={authority},dst=/tmp/flight.xauthority,readonly',
              '-e','XAUTHORITY=/tmp/flight.xauthority']
    cmd+=['modern-arena-tools:0.1','timeout',f'{seconds+30}s','dosbox','-conf','.work/flight/run/watch.conf']
    # No xhost changes, no network, no fullscreen, no audio, bounded runtime.
    result=subprocess.run(cmd).returncode
    if args.campaign and (run/'DEMO.TXT').exists():
        fields=dict(line.split('=',1) for line in (run/'DEMO.TXT').read_text().splitlines() if '=' in line)
        if fields.get('status')!='ok':
            print('Pilot could not start: '+fields.get('error','see DEMO.TXT'),file=sys.stderr)
            result=1
        else: print('Pilot: '+fields.get('save_notice','')+'; last port '+fields.get('saved_planet',''))
    raise SystemExit(result)


if __name__=='__main__': main()
