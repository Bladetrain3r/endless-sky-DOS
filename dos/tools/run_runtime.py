#!/usr/bin/env python3
"""Pinned DJGPP runtime probe. Downloads inputs, compiles/runs only in Docker."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / '.work/toolchain'
WORK = ROOT / '.work/dos-probe'
IMAGE = os.environ.get('DOS_PROBE_IMAGE', 'modern-arena-tools:0.1')
INPUTS = {
    'djgpp-linux64-gcc1220.tar.bz2': (
        'https://github.com/andrewwutw/build-djgpp/releases/download/v3.4/djgpp-linux64-gcc1220.tar.bz2',
        '8464f17017d6ab1b2bb2df4ed82357b5bf692e6e2b7fee37e315638f3d505f00'),
    'csdpmi7b.zip': ('https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7b.zip',
        'deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'),
}
FLAGS = ['-std=gnu++20', '-O2', '-march=i386', '-mtune=i586',
         '-mno-mmx', '-mno-sse', '-mno-sse2']

def capture(args):
    return subprocess.check_output(args, text=True).strip()

def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    for name, (url, digest) in INPUTS.items():
        dest = CACHE/name
        if not dest.exists():
            partial = dest.with_suffix(dest.suffix+'.part')
            with urllib.request.urlopen(url, timeout=60) as src, partial.open('wb') as out:
                while chunk := src.read(1024*1024):
                    out.write(chunk)
            partial.replace(dest)
        if hashlib.sha256(dest.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Input hash mismatch: {name}')
    with zipfile.ZipFile(CACHE/'csdpmi7b.zip') as archive:
        (WORK/'CWSDPMI.EXE').write_bytes(archive.read('bin/CWSDPMI.EXE'))
    docker = ['docker', 'run', '--rm', '--network', 'none', '--user',
              f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp',
              '-v', f'{ROOT}:/work', '-w', '/work', IMAGE]
    if not (CACHE/'djgpp/bin/i586-pc-msdosdjgpp-g++').exists():
        subprocess.run(docker+['python3', '-c',
            "import tarfile; t=tarfile.open('.work/toolchain/djgpp-linux64-gcc1220.tar.bz2'); "
            "t.extractall('.work/toolchain', filter='data')"], check=True)
    compiler = '.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-g++'
    version = capture(docker+[compiler, '--version']).splitlines()[0]
    subprocess.run(docker+[compiler]+FLAGS+['dos/probes/runtime.cpp', '-o',
                                           '.work/dos-probe/RUNTIME.EXE'], check=True)
    (WORK/'RUNTIME.TXT').unlink(missing_ok=True)
    subprocess.run(docker+['sh', '-c',
        'timeout 90s xvfb-run -a dosbox -conf dos/probes/runtime.conf '
        '> .work/dos-probe/runtime.log 2>&1'], check=True, timeout=110)
    raw = (WORK/'RUNTIME.TXT').read_text()
    fields = dict(line.split('=', 1) for line in raw.splitlines() if '=' in line)
    if fields.get('status') != 'ok' or fields.get('allocation_touch_bytes') != '4194304':
        raise RuntimeError(f'Guest probe failed: {raw}')
    if list(WORK.glob('*.SWP')) or list(WORK.glob('*.swp')):
        raise RuntimeError('Unexpected swap file; inspect before trusting memory result')
    report = {'kind': 'dos-cpp-runtime-not-game-memory', 'compiler': version,
        'compiler_flags': FLAGS, 'inputs': INPUTS,
        'image_id': capture(['docker','image','inspect',IMAGE,'--format','{{.Id}}']),
        'config_sha256': hashlib.sha256((ROOT/'dos/probes/runtime.conf').read_bytes()).hexdigest(),
        'source_sha256': hashlib.sha256((ROOT/'dos/probes/runtime.cpp').read_bytes()).hexdigest(),
        'exe_bytes': (WORK/'RUNTIME.EXE').stat().st_size,
        'swap': 'CWSDPMI -s-; no swap file observed', 'raw': fields,
        'limits': 'Tests span/concepts/vector/map/string/exceptions and one touched allocation. '
                  'Not filesystem/threading/complete library coverage or game memory validation. '
                  'Compiler flags govern our object; bundled library instruction set not exhaustively audited.'}
    (ROOT/'dos/reports/runtime-baseline.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()
