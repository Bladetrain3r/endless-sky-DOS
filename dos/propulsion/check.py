#!/usr/bin/env python3
"""Compare native Ship::Move resources with the portable C kernel in Docker/DOSBox."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
from assets import stage
ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/propulsion'
NATIVE = 'endless-sky-dos-native-reference:local'
TOOLS = 'modern-arena-tools:0.1'
SOURCES = ['dos/propulsion/check.c', 'dos/propulsion/propulsion.c', 'dos/motion/motion.c', 'dos/resources/resources.c']
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
        subprocess.run(['python3', str(ROOT / 'dos/propulsion/native.py')], check=True)
    run = WORK / 'run'
    run.mkdir(parents=True, exist_ok=True)
    stage(run)
    provenance = json.loads((WORK / 'native-provenance.json').read_text())
    assert digest(WORK / 'native.bin') == provenance['trace_binary_sha256']
    spec = importlib.util.spec_from_file_location('resource_assets', ROOT / 'dos/resources/assets.py')
    assert spec and spec.loader
    resource_assets = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(resource_assets)
    resource_assets.stage(run)
    shutil.copyfile(WORK / 'native.csv', run / 'NATIVE.CSV')
    shutil.copyfile(WORK / 'native.bin', run / 'NATIVE.BIN')
    flags = ['-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror']
    docker(NATIVE, ['gcc', *flags, '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-no-pie', *SOURCES,
                    '-lm', '-o', '.work/propulsion/check-native'])
    native = docker(NATIVE, ['env', 'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1',
                             'UBSAN_OPTIONS=halt_on_error=1', '.work/propulsion/check-native',
                             '.work/propulsion/run/NATIVE.BIN', '.work/propulsion/run/RESOURCE.DAT', '.work/propulsion/run/PROPULSE.DAT'], check=False)
    (WORK / 'native-check.txt').write_text(native.stdout + native.stderr)
    assert native.returncode == 0 and not native.stderr, (native.stdout, native.stderr)
    dos_flags = flags + ['-march=i386', '-mtune=i586', '-mno-mmx', '-mno-sse', '-mno-sse2']
    docker(TOOLS, ['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc', *dos_flags,
                   *SOURCES, '-lm', '-o', '.work/propulsion/run/PROPULSE.EXE'])
    archive = ROOT / '.work/toolchain/csdpmi7b.zip'
    assert digest(archive) == 'deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (run / 'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base = (ROOT / 'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config = (base + '[autoexec]\nmount c /work/.work/propulsion/run\nc:\nCWSDPMI -s-\n'
              'PROPULSE.EXE NATIVE.BIN RESOURCE.DAT PROPULSE.DAT > RESULT.TXT\nexit\n')
    (run / 'check.conf').write_text(config)
    (run / 'RESULT.TXT').unlink(missing_ok=True)
    output = docker(TOOLS, ['timeout', '60s', 'xvfb-run', '-a', 'dosbox',
                            '-conf', '.work/propulsion/run/check.conf'])
    (run / 'dosbox.log').write_text(output.stdout + output.stderr)
    results = {'native_asan_ubsan': fields(native.stdout),
               'dosbox_16mib_20k': fields((run / 'RESULT.TXT').read_text())}
    assert all(r.get('status') == 'pass' for r in results.values()), results
    report = {
        'schema': 1, 'oracle': provenance, 'results': results,
        'source_sha256': {p: digest(ROOT / p) for p in SOURCES +
                          ['dos/propulsion/propulsion.h', 'dos/motion/motion.h', 'dos/resources/resources.h', 'dos/propulsion/check.py',
                           'dos/propulsion/assets.py']},
        'trace_sha256': digest(run / 'NATIVE.CSV'),
        'trace_binary_sha256': digest(run / 'NATIVE.BIN'),
        'profile_sha256': digest(run / 'PROPULSE.DAT'),
        'resource_profile_sha256': digest(run / 'RESOURCE.DAT'),
        'source_profile': (WORK / 'PROPULSE.DAT').read_text(),
        'staged_profile_format': 'ESPROP2 NUL magic followed by four little-endian binary64 values',
        'dos_flags': dos_flags, 'dos_config': config,
        'tolerance': 'Absolute position <=1e-8, velocity <=1e-10, energy/heat <=1e-9; angle, fired, overheated exact.',
        'scope': 'Full-health stock Sparrow movement through native Ship::Move and optional post-move Energy Blaster ExpendAmmo; synthetic low battery and heat fixtures. No repair/status/fuel/solar/active cooling, reverse engines or whole Engine cadence.',
        'sanitizers': 'ASan+UBSan on portable checker/kernel; original native object build unchanged, unsanitized.',
    }
    (ROOT / 'dos/reports/propulsion-equivalence.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(results, indent=2))
if __name__ == '__main__':
    try: main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(f'{error}\n{error.stdout}\n{error.stderr}')
