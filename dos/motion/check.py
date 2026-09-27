#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare cumulative portable trajectories to actual upstream Ship::Move."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / '.work/motion'
NATIVE_IMAGE = 'endless-sky-dos-native-reference:local'
DOS_IMAGE = 'modern-arena-tools:0.1'
SOURCES = ['dos/motion/motion.c', 'dos/motion/check.c']
NATIVE_FLAGS = ['-std=gnu99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
DOS_FLAGS = ['-std=gnu99', '-O2', '-march=i386', '-mtune=i586',
             '-mno-mmx', '-mno-sse', '-mno-sse2', '-Wall', '-Wextra', '-Werror']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def docker(image, command):
    return subprocess.run([
        'docker', 'run', '--rm', '--network', 'none', '--user', f'{os.getuid()}:{os.getgid()}',
        '-e', 'HOME=/tmp', '-v', f'{ROOT}:/work', '-w', '/work', image, *command,
    ], capture_output=True, text=True, timeout=180, check=True)


def fields(text):
    result = dict(line.split('=', 1) for line in text.splitlines() if '=' in line)
    assert result.get('status') == 'pass', text
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reuse-oracle', action='store_true',
                        help='Use existing local native reference traces')
    args = parser.parse_args()
    if not args.reuse_oracle:
        subprocess.run(['python3', str(ROOT / 'dos/motion/native.py')], check=True)
    oracle = json.loads((WORK / 'provenance.json').read_text())
    assert digest(WORK / 'native.csv') == oracle['csv_sha256']
    assert digest(WORK / 'angles.bin') == oracle['angles_sha256']
    assert digest(ROOT / 'dos/motion/oracle.cpp') == oracle['oracle_source_sha256']
    assert digest(ROOT / 'dos/motion/native.py') == oracle['runner_source_sha256']
    run = WORK / 'run'
    run.mkdir(parents=True, exist_ok=True)
    inputs = SOURCES + ['dos/motion/motion.h', 'dos/motion/check.py']
    hashes = {p: digest(ROOT / p) for p in inputs}
    # Native trace filenames are the oracle's public interface.
    shutil.copyfile(WORK / 'native.csv', run / 'TRACE.CSV')
    shutil.copyfile(WORK / 'angles.bin', run / 'ANGLES.BIN')
    docker(NATIVE_IMAGE, ['gcc', *NATIVE_FLAGS, *SOURCES, '-lm',
                          '-o', '.work/motion/check-native'])
    native = docker(NATIVE_IMAGE, [
        'env', 'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS=halt_on_error=1',
        '.work/motion/check-native', '.work/motion/run/TRACE.CSV', '.work/motion/run/ANGLES.BIN'])
    assert not native.stderr, native.stderr
    native_result = fields(native.stdout)
    docker(DOS_IMAGE, ['.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc',
                       *DOS_FLAGS, *SOURCES, '-lm', '-o', '.work/motion/run/MOTION.EXE'])
    archive = ROOT / '.work/toolchain/csdpmi7b.zip'
    assert digest(archive) == 'deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'
    with zipfile.ZipFile(archive) as z:
        (run / 'CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    base = (ROOT / 'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config = base + ('[autoexec]\nmount c /work/.work/motion/run\nc:\nCWSDPMI -s-\n'
                     'MOTION.EXE TRACE.CSV ANGLES.BIN > RESULT.TXT\nexit\n')
    (run / 'check.conf').write_text(config)
    (run / 'RESULT.TXT').unlink(missing_ok=True)
    dos = docker(DOS_IMAGE, ['timeout', '150s', 'xvfb-run', '-a', 'dosbox',
                             '-conf', '.work/motion/run/check.conf'])
    (run / 'dosbox.log').write_text(dos.stdout + dos.stderr)
    dos_result = fields((run / 'RESULT.TXT').read_text())
    assert not any(p.suffix.lower() == '.swp' for p in run.iterdir())
    assert hashes == {p: digest(ROOT / p) for p in inputs}, 'source changed during checks'
    assert digest(run / 'TRACE.CSV') == oracle['csv_sha256']
    assert digest(run / 'ANGLES.BIN') == oracle['angles_sha256']
    report = {
        'kind': 'bounded-regular-flight-differential-not-complete-ship-port',
        'native': native_result, 'dos': dos_result,
        'oracle': oracle,
        'source_sha256': hashes, 'native_flags': NATIVE_FLAGS, 'dos_flags': DOS_FLAGS,
        'sanitizers': 'ASan+UBSan; leak checking disabled', 'dos_config': config,
        'trace_sha256': digest(run / 'TRACE.CSV'), 'angles_sha256': digest(run / 'ANGLES.BIN'),
        'exe_sha256': digest(run / 'MOTION.EXE'), 'exe_bytes': (run / 'MOTION.EXE').stat().st_size,
        'images': {image: subprocess.check_output(
            ['docker', 'image', 'inspect', '--format', '{{.Id}}', image], text=True).strip()
            for image in (NATIVE_IMAGE, DOS_IMAGE)},
        'limits': ['Healthy supplied regular flight only; no full simulation or graphics cost.',
                   'Benchmark is 64 synthetic ships for 600 ticks, not game FPS.',
                   'State bytes are only stack actor states, not complete guest residency.',
                   'Accepted visual demo unchanged; kernel not integrated into flight scene.'],
    }
    (ROOT / 'dos/reports/motion-equivalence.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'native': native_result, 'dos': dos_result}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f'{exc}\n{exc.stdout or ""}\n{exc.stderr or ""}')
