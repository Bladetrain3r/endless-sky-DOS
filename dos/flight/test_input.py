#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and exercise IRQ1 keyboard input with native checks and DOSBox XTest."""
import hashlib
import json
import re
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/input'
REPORT=ROOT/'dos/reports/flight-input-tests.json'
TOOLS='modern-arena-tools:0.1'
NATIVE='endless-sky-dos-native-reference:local'

def docker(image,command):
    return subprocess.run(['docker','run','--rm','--network','none','--user',
       f'{os.getuid()}:{os.getgid()}','-e','HOME=/tmp','-v',f'{ROOT}:/work',
       '-w','/work',image,*command],check=True,text=True,capture_output=True)

def guest():
    output=RUN/'INPUT.TXT'
    output.unlink(missing_ok=True)
    proc=subprocess.Popen(['dosbox','-conf','.work/flight/input/input.conf'],
                          stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        deadline=time.monotonic()+12
        windows=[]
        while time.monotonic()<deadline and not windows:
            found=subprocess.run(['xdotool','search','--name','DOSBox'],capture_output=True,text=True)
            windows=found.stdout.split()
            if not windows: time.sleep(.1)
        time.sleep(3)
        assert windows,'DOSBox window unavailable'
        subprocess.run(['xdotool','windowfocus','--sync',windows[-1]],check=True)
        def send(*args):
            subprocess.run(['xdotool',*args],check=True)
            time.sleep(.18)
        send('keydown','w'); send('keydown','Left'); send('keyup','w'); send('keyup','Left')
        send('keydown','Up'); send('keydown','s'); send('keyup','Up'); send('keyup','s')
        send('key','r'); send('key','Tab'); send('key','Escape')
        proc.wait(timeout=30)
        rows=output.read_text().splitlines()
        values=[int(x.split('=',1)[1]) for x in rows if x.startswith('keys=')]
        assert rows[-2:]==['reopen=ok','closed=ok'],rows
        expected=[1,5,4,0,1,3,2,0,32,0,64,0,16,0]
        cursor=0
        for value in values:
            if cursor<len(expected) and value==expected[cursor]: cursor+=1
        assert cursor==len(expected),(values,expected)
        print(json.dumps({'guest':'pass','changes':values,'observed':next(x for x in rows if x.startswith('observed=')),'vector_restore':'pass','reopen':'pass'}))
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=3)


def main():
    if '--guest' in sys.argv: return guest()
    RUN.mkdir(parents=True,exist_ok=True)
    native=docker(NATIVE,['sh','-c',
       'gcc -std=c99 -O1 -g -no-pie -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror '
       'dos/flight/input_state.c dos/flight/input_test.c -o .work/flight/input/INPUT_NATIVE && '
       'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 .work/flight/input/INPUT_NATIVE'])
    assert 'decoder=pass' in native.stdout
    djgpp='.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc'
    docker(TOOLS,[djgpp,'-std=gnu99','-O2','-march=i386','-mtune=i586',
       '-mno-mmx','-mno-sse','-mno-sse2','-Wall','-Wextra','-Werror',
       'dos/flight/input.c','dos/flight/input_state.c','dos/flight/input_test.c',
       '-o','.work/flight/input/INPUT.EXE'])
    toolchain='.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-'
    symbols=docker(TOOLS,[toolchain+'nm','-n','.work/flight/input/INPUT.EXE']).stdout
    addresses={name:int(addr,16) for addr,kind,name in
               (line.split() for line in symbols.splitlines() if len(line.split())==3)}
    irq=addresses['_keyboard_irq']; feed=addresses['_input_state_feed']
    irq_size=addresses['_input_open']-irq
    feed_size=addresses['_input_state_keys']-feed
    assert 0<irq_size<4096 and 0<feed_size<4096
    def assembly(start,end):
        return docker(TOOLS,[toolchain+'objdump','-d',f'--start-address={start}',
                             f'--stop-address={end}','.work/flight/input/INPUT.EXE']).stdout
    irq_asm=assembly(irq,addresses['_input_open'])
    feed_asm=assembly(feed,addresses['_input_state_keys'])
    assert re.findall(r'\bcall\s+[^\n]+',irq_asm)==[re.search(r'\bcall\s+[^\n]+',irq_asm).group(0)]
    assert '<_input_state_feed>' in irq_asm and not re.search(r'\bcall\s+',feed_asm)
    with zipfile.ZipFile(ROOT/'.work/toolchain/csdpmi7b.zip') as archive:
        (RUN/'CWSDPMI.EXE').write_bytes(archive.read('bin/CWSDPMI.EXE'))
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    (RUN/'input.conf').write_text(base+'[autoexec]\nmount c /work/.work/flight/input\nc:\n'
          'CWSDPMI -s-\nINPUT.EXE > CONSOLE.TXT\nexit\n')
    dos=docker(TOOLS,['sh','-c','timeout 30s xvfb-run -a python3 dos/flight/test_input.py --guest'])
    data=json.loads(dos.stdout.strip().splitlines()[-1])
    sources=['dos/flight/input.c','dos/flight/input.h','dos/flight/input_state.c',
             'dos/flight/input_state.h','dos/flight/input_test.c','dos/flight/test_input.py']
    hashes={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in sources}
    images={name:subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',name],
             text=True).strip() for name in (TOOLS,NATIVE)}
    REPORT.write_text(json.dumps({'decoder':'pass','djgpp_compile':'pass',
        'irq_code':{'handler_bytes':irq_size,'decoder_bytes':feed_size,
                    'locked_bytes_per_function':4096,'handler_calls':'decoder only',
                    'decoder_calls':'none'},'source_sha256':hashes,'image_ids':images,
        'dosbox_irq1':data,'configuration':'16 MiB, 20,000 fixed DOSBox cycles, CWSDPMI -s-',
        'limits':['XTest software key injection; physical keyboards and unusual controllers untested.']},indent=2)+'\n')
    print(REPORT)

if __name__=='__main__': main()
