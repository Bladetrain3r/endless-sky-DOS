#!/usr/bin/env python3
"""Run the diagnostic in a disposable container; retain one bounded work folder."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
IMAGE = os.environ.get('DOS_PROBE_IMAGE', 'modern-arena-tools:0.1')
WORK = ROOT / '.work/dos-probe'

def command(*args):
    return subprocess.check_output(args, text=True).strip()

def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'FILL.TXT').unlink(missing_ok=True)
    image_id = command('docker', 'image', 'inspect', IMAGE, '--format', '{{.Id}}')
    docker = ['docker', 'run', '--rm', '--network', 'none', '--user',
              f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp',
              '-v', f'{ROOT}:/work', '-w', '/work', IMAGE]
    versions = command(*docker, 'sh', '-c',
                       'dpkg-query -W dosbox nasm xvfb')
    subprocess.run(docker + ['sh', '-c',
        'nasm -f bin dos/probes/vbe_fill.asm -o .work/dos-probe/VBEFILL.COM && '
        'timeout 90s xvfb-run -a dosbox -conf dos/probes/dosbox.conf '
        '> .work/dos-probe/dosbox.log 2>&1'], check=True, timeout=110)
    raw = (WORK / 'FILL.TXT').read_text()
    fields = dict(line.split('=', 1) for line in raw.splitlines() if '=' in line)
    if fields.get('status') != 'ok' or fields.get('sample_check') != 'pass':
        raise RuntimeError(f'DOS probe failed: {raw}')
    ticks = int(fields['bios_ticks_hex'], 16)
    frames = int(fields['frames'])
    if ticks <= 0 or frames != 1200:
        raise RuntimeError('Invalid timing/frame count')
    # BIOS IRQ0 uses the traditional PIT input / 65536; endpoint quantization
    # is roughly +/- one tick over this batch, not per-frame precision.
    hz = 1193182 / 65536
    result = {
        'kind': 'banked-vram-fill-only-not-gameplay',
        'upstream_commit': command('git', '-C', str(ROOT), 'rev-parse', 'HEAD'),
        'image_id': image_id, 'packages': versions.splitlines(),
        'source_sha256': hashlib.sha256((ROOT/'dos/probes/vbe_fill.asm').read_bytes()).hexdigest(),
        'config_sha256': hashlib.sha256((ROOT/'dos/probes/dosbox.conf').read_bytes()).hexdigest(),
        'settings': {'cycles': 20000, 'core': 'normal', 'cputype': 'pentium_slow',
                     'memsize': 16, 'mode': '800x600x8', 'machine': 'svga_s3'},
        'raw': fields, 'elapsed_seconds_approx': ticks/hz,
        'mean_fill_fps_approx': frames*hz/ticks,
        'mean_fill_ms_approx': ticks/hz/frames*1000,
        'limits': 'No game logic, sprites, RAM framebuffer copy, input, or audio. '
                  'Sample checks cover bank endpoints only. No frame-time distribution. '
                  'DOSBox cycles are not a hardware MHz calibration.'
    }
    dest = ROOT/'dos/reports/vbe-fill-baseline.json'
    dest.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
