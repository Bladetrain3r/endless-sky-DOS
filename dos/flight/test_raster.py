#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native sanitizer parity: span and scalar rasterizers, all headings and edges."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
RUN=ROOT/'.work/flight/run'
SOURCES=['dos/flight/raster_test.c','dos/flight/sprite.c','dos/flight/sprite_reference.c']


def main():
    raw=(RUN/'SPARROW.SPR').read_bytes()
    (RUN/'SHORT.SPR').write_bytes(raw[:-1])
    bad=bytearray(raw)
    struct.pack_into('<H',bad,8,65535)
    (RUN/'HUGE.SPR').write_bytes(bad)
    executable=ROOT/'.work/flight/raster-test'
    subprocess.run(['gcc','-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
                    *(str(ROOT/p) for p in SOURCES),'-o',str(executable)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    p=subprocess.run([str(executable)],cwd=RUN,env=env,capture_output=True,text=True,timeout=60)
    assert p.returncode==0 and 'status=pass' in p.stdout,(p.stdout,p.stderr)
    assert not p.stderr,p.stderr
    fields=dict(line.split('=',1) for line in p.stdout.splitlines())
    fields['sanitizers']='ASan+UBSan; leak detection disabled'
    fields['source_sha256']={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in SOURCES}
    (ROOT/'dos/reports/flight-raster-tests.json').write_text(json.dumps(fields,indent=2)+'\n')
    print(json.dumps(fields))


if __name__=='__main__': main()
