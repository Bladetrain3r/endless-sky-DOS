#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Watch the already-built prototype in the reference DOSBox container via local X11."""
import os
from pathlib import Path
import socket
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/run'


def main():
    display=os.environ.get('DISPLAY','')
    if not display.startswith((':','unix:')):
        sys.exit('A local X11/XWayland DISPLAY is needed for the reference window.')
    if not (RUN/'FLIGHT.EXE').is_file():
        sys.exit('Build first: python3 dos/flight/run.py')
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    (RUN/'watch.conf').write_text(base+'[autoexec]\nmount c /work/.work/flight/run\nc:\n'
        'CWSDPMI -s-\nFLIGHT.EXE --demo --seconds 30 > DEMO.TXT\nexit\n')
    cmd=['docker','run','--rm','--network','none','--hostname',socket.gethostname(),'--user',f'{os.getuid()}:{os.getgid()}',
         '-e','HOME=/tmp','-e',f'DISPLAY={display}',
         '--mount','type=bind,src=/tmp/.X11-unix,dst=/tmp/.X11-unix,readonly',
         '--mount',f'type=bind,src={ROOT},dst=/work,readonly',
         '--mount',f'type=bind,src={RUN},dst=/work/.work/flight/run', '-w','/work']
    authority=Path(os.environ.get('XAUTHORITY',str(Path.home()/'.Xauthority')))
    if authority.is_file():
        cmd+=['--mount',f'type=bind,src={authority},dst=/tmp/flight.xauthority,readonly',
              '-e','XAUTHORITY=/tmp/flight.xauthority']
    cmd+=['modern-arena-tools:0.1','timeout','60s','dosbox','-conf','.work/flight/run/watch.conf']
    # No xhost changes, no network, no fullscreen, no audio, bounded runtime.
    raise SystemExit(subprocess.run(cmd).returncode)


if __name__=='__main__': main()
