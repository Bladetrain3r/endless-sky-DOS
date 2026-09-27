#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a small, explicitly fitted Sparrow profile from the qualified native trace."""
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    trace = ROOT / '.work/motion/native.csv'
    provenance = json.loads((ROOT / '.work/motion/provenance.json').read_text())
    digest = hashlib.sha256(trace.read_bytes()).hexdigest()
    assert digest == provenance['csv_sha256'], 'Rebuild the native motion oracle'
    with trace.open() as stream:
        row = next(r for r in csv.DictReader(stream) if r['case'] == 'sparrow_forward' and r['tick'] == '0')
    fields = ['accel', 'reverse_accel', 'turn_rate', 'drag', 'mult']
    values = [float(row[k]) for k in fields]
    assert values[0] > 0 and values[1] == 0 and values[4] == 1
    output = ROOT / '.work/flight/run/PILOT.DAT'
    output.write_text('ESPILOT1 Sparrow\n' + ' '.join(format(v, '.17g') for v in values) + '\n')
    report = {'model': 'Sparrow', 'parameters': dict(zip(fields, values)),
              'native_trace_sha256': digest, 'native_source_commit': provenance['source_commit'],
              'resource_data_sha256': provenance['resource_data_sha256'],
              'file_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'scope': 'One native fitted stock ship; not an outfit or complete ship-data exporter.'}
    (output.parent / 'pilot.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
