#!/usr/bin/env python3
"""Link a bounded pursuit oracle with unchanged native game objects."""
# SPDX-License-Identifier: GPL-3.0-or-later
import csv
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/pursuit'
IMAGE = 'endless-sky-dos-native-reference:local'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def inside():
    work, build = Path('/work/pursuit'), Path('/baseline/build')
    source = Path('/src/dos/pursuit/oracle.cpp')
    obj, binary = work / 'oracle.o', work / 'oracle-native'
    flags = ['-DAVIF_DLL', '-isystem', '/usr/include/SDL2', '-isystem', '/usr/include/AL',
             '-O3', '-DNDEBUG', '-std=c++20', '-flto=auto', '-fno-fat-lto-objects',
             '-Wall', '-pedantic-errors', '-Wold-style-cast', '-fno-rtti']
    subprocess.run(['/usr/bin/c++', *flags, '-I', '/src/source', '-c', str(source), '-o', str(obj)], check=True)
    link = shlex.split((build / 'CMakeFiles/EndlessSky.dir/link.txt').read_text())
    main = 'CMakeFiles/EndlessSky.dir/source/main.cpp.o'
    assert link.count(main) == 1
    link[link.index(main)] = str(obj)
    assert link[link.index('-o') + 1] == 'endless-sky'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    config = work / 'empty-config'
    if config.exists(): shutil.rmtree(config)
    config.mkdir()
    commands, intercepts, profile = (work / n for n in ('commands.csv', 'intercepts.csv', 'PURSUIT.DAT'))
    result = subprocess.run([str(binary), str(commands), str(intercepts), str(profile),
                             '--resources', '/src', '--config', str(config), '--tq-threads', '1'],
                            cwd='/src', capture_output=True, text=True, timeout=180)
    (work / 'native.stdout').write_text(result.stdout)
    (work / 'native.stderr').write_text(result.stderr)
    if result.returncode: raise RuntimeError(f'oracle exit {result.returncode}: {result.stderr[-1500:]}')
    with commands.open(newline='') as f: command_rows = list(csv.DictReader(f))
    with intercepts.open(newline='') as f: intercept_rows = list(csv.DictReader(f))
    stats = dict(item.split('=', 1) for item in result.stdout.strip().split())
    assert len(command_rows) == int(stats['commands']) >= 16
    assert len(intercept_rows) == int(stats['intercepts']) >= 10
    assert profile.stat().st_size == 48 and profile.read_bytes()[:8] == b'ESPURS1\0'
    record = {'schema': 1, 'source_commit': (build.parent / 'source-commit.txt').read_text().strip(),
              'image_id': os.environ['PURSUIT_IMAGE_ID'], 'baseline_binary_sha256': digest(build / 'endless-sky'),
              'oracle_source_sha256': digest(source), 'runner_source_sha256': digest(Path('/src/dos/pursuit/native.py')),
              'oracle_binary_sha256': digest(binary), 'commands_sha256': digest(commands),
              'intercepts_sha256': digest(intercepts), 'profile_sha256': digest(profile),
              'stock_ship_data_sha256': digest(Path('/src/data/human/ships.txt')),
              'stock_outfit_data_sha256': {name: digest(Path('/src/data/human') / name)
                                           for name in ('engines.txt','power.txt','outfits.txt')},
              'behavior_source_sha256': {name: digest(Path('/src/source') / name)
                                         for name in ('AI.cpp', 'AI.h', 'Ship.cpp', 'Ship.h')},
              'profile_values': {k: float(stats[k]) for k in ('accel','reverse','turn','drag','mult')},
              'commands': len(command_rows), 'intercepts': len(intercept_rows), 'compiler_flags': flags,
              'interface': 'Oracle exposes private AI static methods only while compiling replacement main; original game objects are unchanged.'}
    (work / 'provenance.json').write_text(json.dumps(record, indent=2) + '\n')
    print(result.stdout.strip())

def outside():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline = (ROOT / '.work/native/source-commit.txt').read_text().strip()
    guarded = ['source', 'CMakeLists.txt', 'cmake', 'data']
    for extra in ([baseline, '--'], ['--'], ['--cached', '--']):
        subprocess.run(['git', '-C', str(ROOT), 'diff', '--quiet', *extra, *guarded], check=True)
    unknown = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '--others',
                                       '--exclude-standard', '--', *guarded], text=True)
    if unknown: raise RuntimeError('Untracked upstream source/data input')
    image_id = subprocess.check_output(['docker', 'image', 'inspect', '--format', '{{.Id}}', IMAGE], text=True).strip()
    subprocess.run(['docker', 'run', '--rm', '--network', 'none', '--user', f'{os.getuid()}:{os.getgid()}',
                    '--env', 'HOME=/tmp', '--env', f'PURSUIT_IMAGE_ID={image_id}',
                    '--mount', f'type=bind,src={ROOT},dst=/src,readonly',
                    '--mount', f'type=bind,src={ROOT / ".work/native"},dst=/baseline,readonly',
                    '--mount', f'type=bind,src={WORK},dst=/work/pursuit', IMAGE,
                    'python3', '/src/dos/pursuit/native.py', '--inside'], check=True)
if __name__ == '__main__':
    try: inside() if sys.argv[1:] == ['--inside'] else outside()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(f'pursuit oracle failed: {error}')
