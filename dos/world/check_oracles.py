#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare exported map keys/attributes to unmodified native PrintData outputs."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'.work/world'


def quote(value):
    # DataWriter::Quote protocol, not CSV quoting or shell escaping.
    if '"' in value:
        return '`' + value + '`'
    if not value or '`' in value or any(c.isspace() for c in value):
        return '"' + value + '"'
    return value


def orbit_text(system):
    lines = ['system ' + quote(system['name'])]
    h, j = system['arrival_hyper'], system['arrival_jump']
    if h or j:
        if h == j:
            lines.append('\tarrival ' + format(h, '.6g'))
        else:
            lines += ['\tarrival', '\t\tlink ' + format(h, '.6g'), '\t\tjump ' + format(j, '.6g')]
    lines.append('\thabitable ' + format(system['habitable'], '.6g'))
    depths = {}
    for obj in system['objects']:
        depth = depths[obj['parent']] + 1 if obj['parent'] >= 0 else 0
        depths[obj['index']] = depth
        indent = '\t' * (1 + depth)
        lines.append(indent + 'object' + (' ' + quote(obj['planet']) if obj['planet'] else ''))
        indent += '\t'
        if obj['sprite']:
            lines.append(indent + 'sprite ' + quote(obj['sprite']))
        for field in ('distance', 'period', 'offset'):
            if field != 'offset' or obj[field]:
                lines.append(indent + field + ' ' + format(obj[field], '.6g'))
    return '\n'.join(lines) + '\n\n'


def main():
    data = json.loads((WORK/'native.json').read_text())
    results = {}
    for group, singular in [('systems', 'system'), ('planets', 'planet')]:
        rows = sorted(data[group], key=lambda row: row['name'])
        expected = singular + '\n' + ''.join(quote(r['name'])+'\n' for r in rows)
        actual = (WORK/(group+'.txt')).read_text()
        assert actual == expected, group+' keys differ'
        expected = singular+',attributes\n'
        for row in rows:
            expected += quote(row['name'])
            if row['attributes']:
                expected += ',' + ';'.join(quote(a) for a in row['attributes'])
            expected += '\n'
        actual = (WORK/(group+'-attributes.txt')).read_text()
        assert actual == expected, group+' attributes differ'
        results[group] = dict(keys=len(rows), attribute_rows=len(rows), result='exact')
    cases = [('Sol', 'orbits-sol.txt', data)]
    fixture_path = WORK/'fixture-native.json'
    if fixture_path.exists():
        fixture = json.loads(fixture_path.read_text())
        cases += [('DOS Fixture Alpha', 'fixture-orbits-alpha.txt', fixture),
                  ('DOS Fixture Beta', 'fixture-orbits-beta.txt', fixture)]
    results['orbits'] = {}
    for name, filename, world in cases:
        system = next(s for s in world['systems'] if s['name'] == name)
        assert (WORK/filename).read_text() == orbit_text(system), name+' orbit differs'
        results['orbits'][name] = dict(objects=len(system['objects']),
                                      result='exact at PrintData six-significant-digit precision')
    (WORK/'oracle-check.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results))


if __name__ == '__main__':
    main()
