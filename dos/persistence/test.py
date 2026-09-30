#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Qualify codec, store failures and campaign adapter in isolated native/DOS roots."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/persistence/gates'
NATIVE='endless-sky-dos-native-reference:local'
TOOLS='modern-arena-tools:0.1'
BASE=['dos/persistence/state.c','dos/persistence/codec.c']
NAV=['dos/flight/navigation.c','dos/flight/power.c','dos/navigation/approach.c',
     'dos/propulsion/propulsion.c','dos/flight/target_power.c','dos/pursuit/pursuit.c',
     'dos/flight/opponent.c','dos/flight/threat.c','dos/flight/practice.c',
     'dos/flight/pilot.c','dos/shields/shields.c','dos/resources/resources.c',
     'dos/motion/motion.c','dos/projectile/projectile.c','dos/collision/mask.c','dos/damage/damage.c']
TESTS={'codec':BASE+['dos/persistence/codec_test.c'],
       'store':BASE+['dos/persistence/store.c','dos/persistence/store_test.c'],
       'campaign':BASE+['dos/persistence/store.c','dos/flight/campaign.c','dos/flight/campaign_test.c']+NAV}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def docker(image,command,cwd='/work',timeout=180):
    r=subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
      '-e','HOME=/tmp','-v',f'{ROOT}:/work:ro','-v',f'{WORK}:/work/.work/persistence/gates',
      '-w',cwd,image,*command],text=True,capture_output=True,timeout=timeout)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    return r.stdout

def main():
    WORK.mkdir(parents=True,exist_ok=True)
    results={}; flags=['-std=gnu99','-O2','-Wall','-Wextra','-Werror','-I.work/flight/run']
    inputs=set(sum(TESTS.values(),[]))|{str(p.relative_to(ROOT)) for p in (ROOT/'dos/persistence').glob('*.h')}
    inputs|={str(p.relative_to(ROOT)) for p in (ROOT/'dos/flight').glob('*.h')}
    hashes={n:digest(ROOT/n) for n in sorted(inputs)}
    for name,sources in TESTS.items():
        build=WORK/name;build.mkdir(exist_ok=True)
        output=f'.work/persistence/gates/{name}'
        docker(NATIVE,['gcc',*flags,'-g','-no-pie','-fsanitize=address,undefined',
          '-fno-omit-frame-pointer',*sources,'-lm','-o',output+'/native'])
        dosflags=flags+['-march=i386','-mtune=i586','-mno-mmx','-mno-sse','-mno-sse2']
        docker(TOOLS,['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',*dosflags,
          *sources,'-lm','-o',output+'/CHECK.EXE'])
        results[name]={}
        for platform in ('native','dos'):
            with tempfile.TemporaryDirectory(prefix=name+'-'+platform+'-',dir=WORK) as tmp:
                run=Path(tmp);rel=run.relative_to(ROOT);cwd='/work/'+str(rel)
                if name=='campaign':
                    for p in (ROOT/'.work/flight/run').glob('*.DAT'):shutil.copyfile(p,run/p.name)
                    shutil.copyfile(ROOT/'.work/flight/run/MASKS.BIN',run/'MASKS.BIN')
                    (run/'SAVE').mkdir();(run/'SAVE/SEED.DAT').write_bytes(bytes(range(1,33)))
                if platform=='native':
                    text=docker(NATIVE,['env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1',
                       'UBSAN_OPTIONS=halt_on_error=1','/work/'+output+'/native'],cwd,timeout=240)
                else:
                    shutil.copyfile(build/'CHECK.EXE',run/'CHECK.EXE')
                    shutil.copyfile(ROOT/'.work/flight/run/CWSDPMI.EXE',run/'CWSDPMI.EXE')
                    base=(ROOT/'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
                    conf=base+f'[autoexec]\nmount c {cwd}\nc:\nCWSDPMI -s-\nCHECK.EXE > RESULT.TXT\nexit\n'
                    (run/'check.conf').write_text(conf)
                    docker(TOOLS,['timeout','420s','xvfb-run','-a','dosbox','-conf',str(rel/'check.conf')],timeout=440)
                    text=(run/'RESULT.TXT').read_text()
                marker='store tests passed' if name=='store' else 'status=pass'
                assert marker in text,(name,platform,text)
                results[name][platform]=text.strip()
                print(name,platform,'pass',flush=True)
    assert hashes=={n:digest(ROOT/n) for n in sorted(inputs)},'sources changed during qualification'
    report={'results':results,'sources':hashes,'runner_sha256':digest(Path(__file__)),
       'content_manifest_sha256':hashlib.sha256((ROOT/'.work/flight/run/SAVEKEY.json').read_bytes().rstrip(b'\n')).hexdigest(),
       'images':{n:subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',n],text=True).strip() for n in (NATIVE,TOOLS)},
       'native_sanitizers':'ASan/UBSan/leak detection', 'dos_flags':dosflags,
       'limits':'Emulated DOS commit/readback and injected failures; not physical power-loss/FAT/controller qualification. Campaign adapter test is in-process; actual executable restart tested separately.'}
    (ROOT/'dos/reports/persistence-tests.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
