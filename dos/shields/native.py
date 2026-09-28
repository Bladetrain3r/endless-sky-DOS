#!/usr/bin/env python3
"""Link shield oracle against unchanged native Endless Sky objects."""
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
WORK = ROOT / '.work/shields'
IMAGE = 'endless-sky-dos-native-reference:local'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def inside():
    work, build = Path('/work/shields'), Path('/baseline/build')
    source = Path('/src/dos/shields/oracle.cpp')
    obj, binary = work / 'native-oracle.o', work / 'native-oracle'
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
    config = work / 'native-empty-config'
    if config.exists(): shutil.rmtree(config)
    config.mkdir()
    csv_path, profile = work / 'native.csv', work / 'SHIELD.DAT'
    result = subprocess.run([str(binary), str(csv_path), str(profile), '--resources', '/src',
                             '--config', str(config), '--tq-threads', '1'],
                            cwd='/src', capture_output=True, text=True, timeout=180)
    (work / 'native.stdout').write_text(result.stdout)
    (work / 'native.stderr').write_text(result.stderr)
    if result.returncode: raise RuntimeError(f'oracle exit {result.returncode}: {result.stderr[-1500:]}')
    with csv_path.open(newline='') as stream: rows = list(csv.DictReader(stream))
    stats = dict(item.split('=', 1) for item in result.stdout.strip().split())
    assert int(stats['rows']) == len(rows) and len(rows) > 300
    record = {
        'schema': 1, 'source_commit': Path('/baseline/source-commit.txt').read_text().strip(),
        'image_id': os.environ['SHIELD_IMAGE_ID'],
        'baseline_binary_sha256': digest(build / 'endless-sky'),
        'oracle_source_sha256': digest(source),
        'runner_source_sha256': digest(Path('/src/dos/shields/native.py')),
        'oracle_binary_sha256': digest(binary), 'csv_sha256': digest(csv_path),
        'profile_sha256': digest(profile),
        'stock_ship_data_sha256': digest(Path('/src/data/human/ships.txt')),
        'stock_outfit_data_sha256': {name: digest(Path('/src/data/human') / name) for name in
            ('power.txt', 'engines.txt', 'outfits.txt')},
        'behavior_source_sha256': {name: digest(Path('/src/source') / name) for name in
            ('Ship.cpp', 'Ship.h', 'ship/ResourceLevels.cpp', 'ship/ResourceLevels.h',
             'ship/ShipAttributeCache.cpp', 'ship/ShipAttributeCache.h')},
        'compiler_flags': flags, 'rows': len(rows), 'cases': sorted({r['case'] for r in rows}),
        'stock_profile': {k: float(v) for k, v in stats.items() if k not in ('rows', 'cases')},
        'fields': list(rows[0]),
        'interface': 'Loaded Sparrow copies call unchanged native Ship::DoGeneration. Private members exposed to oracle compilation only; linked game objects unchanged.',
    }
    (work / 'native-provenance.json').write_text(json.dumps(record, indent=2) + '\n')
    print(result.stdout.strip())

def outside():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline = (ROOT / '.work/native/source-commit.txt').read_text().strip()
    guarded = ['source', 'CMakeLists.txt', 'cmake', 'data']
    for extra in ([baseline, '--'], ['--'], ['--cached', '--']):
        subprocess.run(['git', '-C', str(ROOT), 'diff', '--quiet', *extra, *guarded], check=True)
    unknown = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '--others',
                                       '--exclude-standard', '--', *guarded], text=True)
    if unknown: raise RuntimeError('Untracked upstream source/data input; rebuild native reference')
    image_id = subprocess.check_output(['docker', 'image', 'inspect', '--format', '{{.Id}}', IMAGE], text=True).strip()
    subprocess.run(['docker', 'run', '--rm', '--network', 'none', '--user', f'{os.getuid()}:{os.getgid()}',
                    '--env', 'HOME=/tmp', '--env', f'SHIELD_IMAGE_ID={image_id}',
                    '--mount', f'type=bind,src={ROOT},dst=/src,readonly',
                    '--mount', f'type=bind,src={ROOT / ".work/native"},dst=/baseline,readonly',
                    '--mount', f'type=bind,src={WORK},dst=/work/shields', IMAGE,
                    'python3', '/src/dos/shields/native.py', '--inside'], check=True)
if __name__ == '__main__':
    try: inside() if sys.argv[1:] == ['--inside'] else outside()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(f'shield oracle failed: {error}')
