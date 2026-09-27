#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Watch the already-built prototype in the reference DOSBox container via local X11."""
import argparse
import os
from pathlib import Path
import socket
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/run'


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--stationary',action='store_true',help='keep the camera fixed for comparison')
    parser.add_argument('--invulnerable-target',action='store_true',help='retain the indestructible comparison target')
    parser.add_argument('--stationary-target',action='store_true',help='retain the fixed practice target')
    parser.add_argument('--accepted',action='store_true',help='compare preserved pre-scale flight build')
    parser.add_argument('--pilot',action='store_true',help='fly the stock Sparrow with held keys')
    parser.add_argument('--seconds',type=int,default=None,help='duration1..120seconds; default30watch/120pilot')
    args=parser.parse_args()
    run=ROOT/'.work/flight/accepted-0636c12dc' if args.accepted else RUN
    seconds=args.seconds if args.seconds is not None else (120 if args.pilot else 30)
    if not 1<=seconds<=120: parser.error('seconds must be1..120')
    if args.pilot and args.stationary: parser.error('use Tab during pilot mode to switch camera')
    if args.invulnerable_target and (not args.pilot or args.accepted): parser.error('invulnerable-target requires current --pilot build')
    if args.stationary_target and (not args.pilot or args.accepted): parser.error('stationary-target requires current --pilot build')
    camera=' --pilot' if args.pilot else ('' if args.stationary else ' --camera')
    if args.stationary_target: camera+=' --stationary-target'
    if args.invulnerable_target: camera+=' --invulnerable-target'
    display=os.environ.get('DISPLAY','')
    if not display.startswith((':','unix:')):
        sys.exit('A local X11/XWayland DISPLAY is needed for the reference window.')
    if not (run/'FLIGHT.EXE').is_file():
        sys.exit('Build first: python3 dos/flight/run.py')
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
    raise SystemExit(subprocess.run(cmd).returncode)


if __name__=='__main__': main()
