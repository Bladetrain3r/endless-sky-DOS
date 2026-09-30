#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual executable/IRQ save and restart checks; disposable pilot, no user saves."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from fixture import native_row
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/persistence/application'
IMAGE='modern-arena-tools:0.1'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def fields(p):return dict(x.split('=',1) for x in p.read_text().splitlines() if '=' in x)
def guest(run):
    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    outputs={}
    def launch(label,actions):
        out=run/(label+'.TXT');out.unlink(missing_ok=True)
        conf=base+f'[autoexec]\nmount c {run}\nc:\nCWSDPMI -s-\nFLIGHT.EXE --campaign --demo --seconds 120 > {label}.TXT\nexit\n'
        (run/'app.conf').write_text(conf)
        proc=subprocess.Popen(['dosbox','-conf',str(run/'app.conf')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            window=None;deadline=time.monotonic()+30
            while time.monotonic()<deadline and proc.poll() is None:
                found=subprocess.run(['xdotool','search','--name','DOSBox'],capture_output=True,text=True)
                for candidate in found.stdout.split():
                    geom=subprocess.run(['xdotool','getwindowgeometry','--shell',candidate],capture_output=True,text=True).stdout
                    if 'WIDTH=800\n' in geom and 'HEIGHT=600\n' in geom:window=candidate;break
                if window:break
                time.sleep(.1)
            if actions is not None:
                assert window,'flight window did not open: '+label
                subprocess.run(['xdotool','windowfocus','--sync',window],check=True);time.sleep(.5)
                for action,key,pause in actions:
                    subprocess.run(['xdotool',action,key],check=True);time.sleep(pause)
                subprocess.run(['xdotool','key','Escape'],check=True)
            proc.wait(timeout=25)
            result=fields(out);outputs[label]=result
            if actions is not None:assert result.get('status')=='ok',result
            print(label,result.get('status'),result.get('saved_planet'),flush=True)
            return result
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=3)
    a=launch('FIRST',[('keydown','F5',.8),('keyup','F5',.2),('keydown','r',.5),('keyup','r',.2)])
    assert a['nav_phase']=='2' and a['save_generation']=='2' and a['pilot_cargo']=='0',a
    b=launch('TRIP',[('key','l',2.),('key','F5',.3),('key','p',.2),('key','l',18.)])
    assert b['nav_phase']=='2' and b['saved_planet']=='planet:Luna' and b['save_rejections']=='1',b
    assert b['save_generation']=='4' and b['save_dirty']=='0',b
    c=launch('RELOAD',[('key','F5',.3)])
    assert c['nav_selected']=='1' and c['nav_phase']=='2' and c['save_generation']=='5',c
    for key in ('pilot_id','ship_id','pilot_credits','pilot_cargo','pilot_date'):
        assert a[key]==b[key]==c[key],key
    save=run/'SAVE'
    # Corrupt newest (slot0 generation5): older intact Luna generation4 must load.
    newest=save/'PILOT0.SAV';good=save/'PILOT1.SAV';old=digest(good)
    original=newest.read_bytes();newest.write_bytes(original[:-5])
    r=launch('RECOVER',[])
    assert r['save_recovered']=='1' and r['save_generation']=='4' and r['saved_planet']=='planet:Luna',r
    assert digest(good)==old and newest.read_bytes()==original[:-5]
    # Recognizable future format must block, preserving both slots.
    future=bytearray(original);future[8:12]=(2).to_bytes(4,'little');newest.write_bytes(future)
    before={p.name:digest(p) for p in save.glob('*.SAV')}
    bad=launch('FUTURE',None)
    assert bad['status']=='failed' and bad['error']=='unsupported',bad
    assert before=={p.name:digest(p) for p in save.glob('*.SAV')}
    # Independent Python envelope from native after-purchase trace; exercise a
    # nonempty hold through actual startup and another process restart.
    manifest=(ROOT/'.work/flight/run/SAVEKEY.json').read_bytes().rstrip(b'\n')
    content=hashlib.sha256(manifest).digest()
    for p in save.glob('*.SAV'):p.unlink()  # Only this disposable test pilot.
    (save/'PILOT0.SAV').write_bytes(native_row('after_purchase',content,6))
    cargo=launch('CARGO',[('key','F5',.3)])
    again=launch('CARGO2',[])
    assert cargo['pilot_cargo']==again['pilot_cargo']=='3'
    assert cargo['pilot_credits']==again['pilot_credits']=='8485'
    assert cargo['ship_id']==again['ship_id']=='10000000000040008000000000000001'
    assert cargo['save_generation']==again['save_generation']=='7'
    assert cargo['saved_planet']==again['saved_planet']=='planet:Earth'
    assert not list(run.glob('*.SWP'))
    (run/'results.json').write_text(json.dumps(outputs,indent=2)+'\n')

def main():
    if len(sys.argv)>1:return guest(Path(sys.argv[1]))
    WORK.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pilot-',dir=WORK) as tmp:
        run=Path(tmp)
        for p in (ROOT/'.work/flight/run').iterdir():
            if p.suffix in ('.DAT','.PAK','.SPR','.PAL','.LUT','.BIN') or p.name in ('FLIGHT.EXE','CWSDPMI.EXE'):
                shutil.copyfile(p,run/p.name)
        (run/'SAVE').mkdir();(run/'SAVE/SEED.DAT').write_bytes(bytes(range(1,33)))
        inside='/work/'+str(run.relative_to(ROOT))
        subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
           '-e','HOME=/tmp','-v',f'{ROOT}:/work:ro','-v',f'{WORK}:/work/.work/persistence/application',
           '-w','/work',IMAGE,'timeout','150s','xvfb-run','-a','python3','dos/persistence/test_app.py',inside],check=True,timeout=180)
        result=json.loads((run/'results.json').read_text())
        report={'results':result,'exe_sha256':digest(run/'FLIGHT.EXE'),'test_sha256':digest(Path(__file__)),
           'fixture_source_sha256':digest(ROOT/'dos/persistence/fixture.py'),
           'native_trace_sha256':digest(ROOT/'dos/persistence/native-trace.csv'),
           'manifest_sha256':hashlib.sha256((ROOT/'.work/flight/run/SAVEKEY.json').read_bytes().rstrip(b'\n')).hexdigest(),
           'scope':'Actual DOS executable/IRQ: first creation, held F5/reset, departure, in-flight rejection, Luna autosave, separate-process restart, older-slot recovery, unsupported-version refusal; native after-purchase nonempty cargo/credits/identity across two launches.',
           'limits':'Disposable development pilot. Physical power failure and full campaign semantics not covered.'}
        (ROOT/'dos/reports/persistence-application.json').write_text(json.dumps(report,indent=2)+'\n')
        shutil.copyfile(run/'FRAME.IDX',WORK/'FRAME.IDX')
if __name__=='__main__':main()
