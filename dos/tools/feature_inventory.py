#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reproducible survey index, not an Endless Sky parser or parity test."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = '061a9461a93898fb691504536589d1dcddc5d79b'


def inventory():
    totals, files = Counter(), []
    for path in sorted((ROOT / 'data').rglob('*.txt')):
        raw = path.read_bytes()
        headers = Counter()
        for line in raw.decode('utf-8-sig').splitlines():
            if not line or line[0].isspace() or line.startswith('#'):
                continue
            token = re.match(r'("[^"]*"|`[^`]*`|[^\s]+)', line)
            if token:
                headers[token[0].strip('"`')] += 1
        totals.update(headers)
        files.append({'path': str(path.relative_to(ROOT)),
                      'sha256': hashlib.sha256(raw).hexdigest(),
                      'headers': dict(sorted(headers.items()))})
    return {
        'kind': 'static-vanilla-survey-inventory',
        'reference_commit': REFERENCE,
        'limits': 'Counts unindented declaration headers in data/**/*.txt, including '
                  'variants, overrides and deprecated data. Not unique resolved objects, '
                  'playable story counts, a semantic parser, or evidence of DOS support. '
                  'Hashes bind the files read; the reference label alone does not verify them.',
        'source_files': len(files), 'declaration_headers': sum(totals.values()),
        'by_header': dict(sorted(totals.items())), 'files': files,
    }


if __name__ == '__main__':
    result = inventory()
    output = ROOT / 'dos/reports/vanilla-feature-inventory.json'
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'files'}, indent=2))
