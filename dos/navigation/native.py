#!/usr/bin/env python3
"""Extract Sparrow approach cases from unchanged native game objects."""
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/navigation'
IMAGE='endless-sky-dos-native-reference:local'
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def inside():
    work=Path('/work/navigation'); build=Path('/baseline/build')
    source=Path('/src/dos/navigation/oracle.cpp')
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
    config=work/'empty-config'; config.mkdir(exist_ok=True)
    trace=work/'commands.csv'; profile=work/'APPROACH.DAT'; planets=work/'PLANETS.json'
    run=subprocess.run([str(binary),str(trace),str(profile),str(planets),
         '--resources','/src','--config',str(config),'--tq-threads','1'],
         cwd='/src',capture_output=True,text=True,timeout=180)
    (work/'native.stdout').write_text(run.stdout)
    (work/'native.stderr').write_text(run.stderr)
    if run.returncode: raise RuntimeError(f'oracle exit {run.returncode}: {run.stderr[-1500:]}')
    assert profile.stat().st_size==48 and profile.read_bytes()[:8]==b'ESPURS1\0'
    p=json.loads(planets.read_text()); assert len(p['planets'])==2
    record={'schema':1,'source_commit':(build.parent/'source-commit.txt').read_text().strip(),
       'oracle_source_sha256':digest(source),'runner_source_sha256':digest(Path('/src/dos/navigation/native.py')),
       'trace_sha256':digest(trace),'profile_sha256':digest(profile),'planets_sha256':digest(planets),
       'behavior_source_sha256':{n:digest(Path('/src/source')/n) for n in
           ('AI.cpp','AI.h','Ship.cpp','Ship.h','Planet.cpp','Planet.h','Port.cpp','Port.h')},
       'stock_ship_data_sha256':digest(Path('/src/data/human/ships.txt')),
       'stock_outfit_data_sha256':{n:digest(Path('/src/data/human')/n) for n in
           ('engines.txt','power.txt','outfits.txt')},
       'compiler_flags':flags,'oracle_stdout':run.stdout.strip()}
    (work/'provenance.json').write_text(json.dumps(record,indent=2)+'\n')
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
       '--mount',f'type=bind,src={WORK},dst=/work/navigation',IMAGE,
       'python3','/src/dos/navigation/native.py','--inside'],check=True)
if __name__=='__main__':
    try: inside() if sys.argv[1:]==['--inside'] else outside()
    except (AssertionError,OSError,RuntimeError,subprocess.SubprocessError) as e:
        sys.exit(f'Approach oracle failed: {e}')
