#!/usr/bin/env python3
"""Reproduce content-store A/B in DOS; downloads only verified SQLite source."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.request
import zipfile

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'.work/storage'
IMAGE=os.environ.get('DOS_PROBE_IMAGE','modern-arena-tools:0.1')
SQLITE_URL='https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip'
SQLITE_ZIP_SHA256='1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032'
SQLITE_SOURCE_SHA3='9145255e83da6529e70121ee4d7a4c88fe83ca4511da0c9ed13d10842df36782'
CWSDPMI_SHA256='deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect():
    run=WORK/'run'
    corpus=json.loads((run/'corpus.json').read_text())
    methods={}
    for name in ['PACK','PACK64','SQL128','SQL32']:
        text=(run/(name+'.TXT')).read_text()
        fields=dict(line.split('=',1) for line in text.splitlines() if '=' in line)
        if fields.get('status')!='ok':
            raise RuntimeError(f'{name} failed: {text}')
        for phase,trace in [('seq','SEQ'),('random','RAND'),('hot','HOT')]:
            for key in ['queries','payload_bytes']:
                if int(fields[phase+'_'+key])!=corpus['traces'][trace][key]:
                    raise RuntimeError(f'{name}: missing or wrong {phase} workload')
            if float(fields[phase+'_ms'])<=0:
                raise RuntimeError('Invalid guest timer result')
        methods[name]=fields
    for name,entry in corpus['artifacts'].items():
        if sha(run/name)!=entry['sha256']:
            raise RuntimeError(f'Content/trace changed during run: {name}')
    if any(p.suffix.lower()=='.swp' for p in run.iterdir()):
        raise RuntimeError('Unexpected swap file')
    sources={str(p.relative_to(ROOT)):sha(p) for p in sorted((ROOT/'dos/storage').iterdir())
             if p.is_file() and p.suffix in ('.c','.h','.py','.sh','.conf')}
    result={'kind':'dos-readonly-raw-content-storage-comparison',
        'source_commit':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
        'image_id':subprocess.check_output(['docker','image','inspect',IMAGE,'--format','{{.Id}}'],text=True).strip(),
        'sqlite_url':SQLITE_URL, 'sqlite_source_sha3_256':SQLITE_SOURCE_SHA3,
        'settings':{'memsize':16,'cycles':20000,'core':'normal','cputype':'pentium_slow',
                    'swap':'CWSDPMI -s-','sqlite_heap_cap_bytes':1048576},
        'corpus':corpus,'sources_sha256':sources,'methods':methods,
        'executables':{n:{'bytes':(run/n).stat().st_size,'sha256':sha(run/n)}
                       for n in ['PACK.EXE','SQL.EXE']},
        'limits':['Raw text blocks, not parsed world objects or mission execution.',
          'Read-only single-owner immutable corpus: no save writes or crash recovery.',
          'Timed phases include trace reads and payload hashing in both backends.',
          'Store read counters are logical stdio/VFS reads, not host physical I/O.',
          'Same DOSBox process; host caches not flushed. Not a spinning-disk latency test.',
          'Memory counters exclude stdio/libc/stack/code. DPMI pages are snapshots, not peaks.',
          'Dense manifest-local IDs favor direct indexing; arbitrary SQL queries not evaluated.']}
    (ROOT/'dos/reports/storage-baseline.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'records':corpus['records'],'methods':methods,'executables':result['executables']},indent=2))


def main():
    if not (ROOT/'.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc').exists():
        raise SystemExit('First run python3 dos/tools/run_runtime.py to qualify/download toolchain.')
    WORK.mkdir(parents=True,exist_ok=True)
    archive=WORK/'sqlite.zip'
    if not archive.exists():
        with urllib.request.urlopen(SQLITE_URL,timeout=60) as r:
            archive.write_bytes(r.read())
    if sha(archive)!=SQLITE_ZIP_SHA256:
        raise RuntimeError('SQLite archive checksum mismatch')
    (WORK/'sqlite').mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        for base in ['sqlite3.c','sqlite3.h']:
            item=next(n for n in z.namelist() if n.endswith('/'+base))
            (WORK/'sqlite'/base).write_bytes(z.read(item))
    if hashlib.sha3_256((WORK/'sqlite/sqlite3.c').read_bytes()).hexdigest()!=SQLITE_SOURCE_SHA3:
        raise RuntimeError('SQLite published source checksum mismatch')
    docker=['docker','run','--rm','--network','none','--user',f'{os.getuid()}:{os.getgid()}',
            '-e','HOME=/tmp','-v',f'{ROOT}:/work','-w','/work',IMAGE]
    subprocess.run(docker+['python3','dos/storage/make_corpus.py'],check=True)
    subprocess.run(docker+['sh','dos/storage/build.sh'],check=True,timeout=180)
    dpmi=ROOT/'.work/toolchain/csdpmi7b.zip'
    if sha(dpmi)!=CWSDPMI_SHA256: raise RuntimeError('DPMI archive checksum mismatch')
    with zipfile.ZipFile(dpmi) as z:
        (WORK/'run/CWSDPMI.EXE').write_bytes(z.read('bin/CWSDPMI.EXE'))
    for name in ['PACK','PACK64','SQL128','SQL32']:
        (WORK/'run'/(name+'.TXT')).unlink(missing_ok=True)
    subprocess.run(docker+['sh','-c',
        'timeout 180s xvfb-run -a dosbox -conf dos/storage/dosbox.conf '
        '> .work/storage/dosbox.log 2>&1'],check=True,timeout=200)
    collect()

if __name__=='__main__':
    main()
