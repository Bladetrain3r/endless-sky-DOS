#!/usr/bin/env python3
"""Compare native Ship firing resources with the portable C kernel in Docker/DOSBox."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
from assets import stage
ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/resources'
NATIVE = 'endless-sky-dos-native-reference:local'
TOOLS = 'modern-arena-tools:0.1'
SOURCES = ['dos/resources/check.c', 'dos/resources/resources.c']
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def docker(image, args, check=True):
    return subprocess.run(['docker', 'run', '--rm', '--network', 'none',
        '--user', f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp',
        '-v', f'{ROOT}:/work', '-w', '/work', image, *args],
        check=check, capture_output=True, text=True, timeout=180)
def fields(s): return dict(line.split('=', 1) for line in s.splitlines() if '=' in line)
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reuse-oracle', action='store_true')
    args = parser.parse_args()
    if not args.reuse_oracle:
        subprocess.run(['python3', str(ROOT / 'dos/resources/native.py')], check=True)
    run = WORK / 'run'
    run.mkdir(parents=True, exist_ok=True)
    stage(run)
    shutil.copyfile(WORK / 'native.csv', run / 'NATIVE.CSV')
    flags = ['-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror']
    docker(NATIVE, ['gcc', *flags, '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-no-pie', *SOURCES,
                    '-lm', '-o', '.work/resources/check-native'])
    native = docker(NATIVE, ['env', 'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1',
                             'UBSAN_OPTIONS=halt_on_error=1', '.work/resources/check-native',
                             '.work/resources/run/NATIVE.CSV', '.work/resources/run/RESOURCE.DAT'], check=False)
    (WORK / 'native-check.txt').write_text(native.stdout + native.stderr)
    assert native.returncode == 0 and not native.stderr, (native.stdout, native.stderr)
    dos_flags = flags + ['-march=i386', '-mtune=i586', '-mno-mmx', '-mno-sse', '-mno-sse2']
    docker(TOOLS, ['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc', *dos_flags,
                   *SOURCES, '-lm', '-o', '.work/resources/run/RESOURCE.EXE'])
    archive = ROOT / '.work/toolchain/csdpmi7b.zip'
    assert digest(archive) == 'deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (run / 'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base = (ROOT / 'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config = (base + '[autoexec]\nmount c /work/.work/resources/run\nc:\nCWSDPMI -s-\n'
              'RESOURCE.EXE NATIVE.CSV RESOURCE.DAT > RESULT.TXT\nexit\n')
    (run / 'check.conf').write_text(config)
    (run / 'RESULT.TXT').unlink(missing_ok=True)
    output = docker(TOOLS, ['timeout', '60s', 'xvfb-run', '-a', 'dosbox',
                            '-conf', '.work/resources/run/check.conf'])
    (run / 'dosbox.log').write_text(output.stdout + output.stderr)
    results = {'native_asan_ubsan': fields(native.stdout),
               'dosbox_16mib_20k': fields((run / 'RESULT.TXT').read_text())}
    assert all(r.get('status') == 'pass' for r in results.values()), results
    provenance = json.loads((WORK / 'native-provenance.json').read_text())
    report = {
        'schema': 1, 'oracle': provenance, 'results': results,
        'source_sha256': {p: digest(ROOT / p) for p in SOURCES +
                          ['dos/resources/resources.h', 'dos/resources/check.py',
                           'dos/resources/assets.py']},
        'trace_sha256': digest(run / 'NATIVE.CSV'),
        'profile_sha256': digest(run / 'RESOURCE.DAT'),
        'profile': (run / 'RESOURCE.DAT').read_text(),
        'dos_flags': dos_flags, 'dos_config': config,
        'tolerance': 'Absolute energy/heat error <=1e-9; can_fire, fired, overheated exact.',
        'scope': 'Full-health stock Sparrow outfitting and one Energy Blaster; direct native Ship::DoGeneration, CanFire and ExpendAmmo with synthetic capacity/generation/heat stress. No movement, repair/status/fuel/solar/active cooling or hardpoint cooldown.',
        'sanitizers': 'ASan+UBSan on portable checker/kernel; original native object build unchanged, unsanitized.',
    }
    (ROOT / 'dos/reports/resource-equivalence.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(results, indent=2))
if __name__ == '__main__':
    try: main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(f'{error}\n{error.stdout}\n{error.stderr}')
