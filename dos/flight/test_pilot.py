#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run inside the native-reference container from the project root."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

SOURCES = ['dos/flight/pilot_test.c', 'dos/flight/pilot.c', 'dos/flight/scene.c',
           'dos/flight/sprite.c', 'dos/motion/motion.c']
FLAGS = ['-std=gnu99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
subprocess.run(['gcc', *FLAGS, *SOURCES, '-lm', '-o', '.work/flight/pilot-test'], check=True)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
bad = Path('.work/flight/bad-pilot.dat')
cases = ['ESPILOT1 Sparrow\nnan 0 2 .01 1\n', 'ESPILOT1 Sparrow\n',
         'ESPILOT1 Sparrow\n.1 0 2 .01 1 garbage\n', 'ESPILOT1 Sparrow\n.1 1 2 .01 1\n']
for case in cases:
    bad.write_text(case)
    result = subprocess.run(['.work/flight/pilot-test'], env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0 and 'status=pass' in result.stdout and not result.stderr, result
bad.unlink()
report = dict(line.split('=', 1) for line in result.stdout.splitlines())
report['malformed_profiles'] = len(cases)
report['source_sha256'] = {p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in SOURCES}
report['sanitizers'] = 'ASan+UBSan; leak checking disabled'
Path('dos/reports/flight-pilot-tests.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
