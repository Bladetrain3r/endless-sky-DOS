#!/usr/bin/env python3
"""Exercise native landed save/reload and transaction contracts in an isolated config."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/persistence'
IMAGE='endless-sky-dos-native-reference:local'
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def inside():
    work=Path('/work/persistence'); build=Path('/baseline/build')
    source=Path('/src/dos/persistence/oracle.cpp')
    obj=work/'oracle.o'; binary=work/'oracle-native'
    flags=['-DAVIF_DLL','-isystem','/usr/include/SDL2','-isystem','/usr/include/AL',
           '-O3','-DNDEBUG','-std=c++20','-flto=auto','-fno-fat-lto-objects',
           '-Wall','-pedantic-errors','-Wold-style-cast','-fno-rtti']
    subprocess.run(['/usr/bin/c++',*flags,'-I','/src/source','-c',str(source),'-o',str(obj)],check=True)
    link=shlex.split((build/'CMakeFiles/EndlessSky.dir/link.txt').read_text())
    main='CMakeFiles/EndlessSky.dir/source/main.cpp.o'
    assert link.count(main)==1
    link[link.index(main)]=str(obj)
    assert link[link.index('-o')+1]=='endless-sky'
    link[link.index('-o')+1]=str(binary)
    subprocess.run(link,cwd=build,check=True)
    import tempfile
    with tempfile.TemporaryDirectory(prefix='config-',dir=work) as config:
        run=subprocess.run([str(binary),str(work),'--resources','/src','--config',config,'--tq-threads','1'],
             cwd='/src',capture_output=True,text=True,timeout=180)
        (work/'native.stdout').write_text(run.stdout)
        (work/'native.stderr').write_text(run.stderr)
        if run.returncode: raise RuntimeError(f'oracle exit {run.returncode}: {run.stderr[-1500:]}')
    expected=('purchase_reload','next_action','transaction_isolation','quantity_clamps','pending_sale_reload','save_eligibility')
    assert all(key+'=pass' in run.stdout for key in expected),run.stdout
    record={'schema':1,'kind':'native-persistence-contract-not-DOS-save-support',
        'source_commit':(build.parent/'source-commit.txt').read_text().strip(),
        'oracle_source_sha256':digest(source),'runner_source_sha256':digest(Path('/src/dos/persistence/native.py')),
        'behavior_source_sha256':{n:digest(Path('/src/source')/n) for n in
            ('PlayerInfo.cpp','Ship.cpp','PilotProfile.cpp','DataWriter.cpp','Files.cpp','TradingPanel.cpp','GameData.cpp','CargoHold.cpp','Account.cpp')},
        'data_sha256':{str(p.relative_to('/src')):digest(p) for p in sorted(Path('/src/data').rglob('*.txt'))},
        'checks':{key:'pass' for key in expected},'compiler_flags':flags,
        'artifact_sha256':{p.name:digest(p) for p in sorted(work.iterdir()) if p.suffix in ('.TXT','.csv')},
        'limits':'Native fixture only, one stock Sparrow at Earth, Food transactions and selected normalized fields. '
                 'No DOS codec, interrupted-write durability, full campaign equality or native interchange is qualified.'}
    (work/'report.json').write_text(json.dumps(record,indent=2)+'\n')
    print(run.stdout.strip())
def outside():
    WORK.mkdir(parents=True,exist_ok=True)
    baseline=(ROOT/'.work/native/source-commit.txt').read_text().strip()
    guarded=['source','CMakeLists.txt','cmake','data']
    for extra in ([baseline,'--'],['--'],['--cached','--']):
        subprocess.run(['git','-C',str(ROOT),'diff','--quiet',*extra,*guarded],check=True)
    unknown=subprocess.check_output(['git','-C',str(ROOT),'ls-files','--others',
                                      '--exclude-standard','--',*guarded],text=True)
    if unknown: raise RuntimeError('Untracked upstream input')
    subprocess.run(['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
       '--env','HOME=/tmp','--mount',f'type=bind,src={ROOT},dst=/src,readonly',
       '--mount',f'type=bind,src={ROOT/".work/native"},dst=/baseline,readonly',
       '--mount',f'type=bind,src={WORK},dst=/work/persistence',IMAGE,
       'python3','/src/dos/persistence/native.py','--inside'],check=True)
    record=json.loads((WORK/'report.json').read_text())
    record['container_image_id']=subprocess.check_output(
        ['docker','image','inspect','--format','{{.Id}}',IMAGE],text=True).strip()
    (WORK/'report.json').write_text(json.dumps(record,indent=2)+'\n')
    import shutil
    shutil.copyfile(WORK/'report.json', ROOT/'dos/reports/native-persistence-contract.json')
    shutil.copyfile(WORK/'trace.csv', ROOT/'dos/persistence/native-trace.csv')
if __name__=='__main__':
    try: inside() if sys.argv[1:]==['--inside'] else outside()
    except (AssertionError,OSError,RuntimeError,subprocess.SubprocessError) as e:
        sys.exit(f'Persistence oracle failed: {e}')
