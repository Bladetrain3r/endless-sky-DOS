#!/usr/bin/env python3
"""Compare native Star Barge generation traces with existing generic DOS kernels."""
# SPDX-License-Identifier: GPL-3.0-or-later
import csv
import ctypes as C
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = Path('/work/opponent_profile') if Path('/work/opponent_profile').is_dir() else ROOT / '.work/opponent_profile'

class ResourceProfile(C.Structure):
    _fields_ = [(name, C.c_double) for name in ('capacity', 'generation', 'heat_generation',
                                                'dissipation', 'max_heat', 'shot_energy', 'shot_heat')]
class ResourceState(C.Structure):
    _fields_ = [('energy', C.c_double), ('heat', C.c_double), ('overheated', C.c_int)]
class ShieldProfile(C.Structure):
    _fields_ = [(name, C.c_double) for name in ('capacity', 'rate', 'energy_cost', 'heat_cost',
                                                'delayed_rate', 'delayed_energy_cost', 'delayed_heat_cost')]
    _fields_ += [('delay', C.c_int), ('depleted_delay', C.c_int)]
class ShieldState(C.Structure):
    _fields_ = [('shields', C.c_double), ('delay', C.c_int)]

def check():
    library = WORK / 'generic-check.so'
    subprocess.run(['/usr/bin/cc', '-std=c11', '-O2', '-fPIC', '-shared',
                    str(ROOT / 'dos/resources/resources.c'), str(ROOT / 'dos/shields/shields.c'),
                    '-lm', '-o', str(library)], check=True)
    lib = C.CDLL(str(library))
    lib.resource_load.argtypes = [C.POINTER(ResourceProfile), C.c_char_p]
    lib.shield_load.argtypes = [C.POINTER(ShieldProfile), C.c_char_p]
    lib.resource_tick_disabled.argtypes = [C.POINTER(ResourceState), C.POINTER(ResourceProfile), C.c_int]
    lib.shield_tick.argtypes = [C.POINTER(ShieldState), C.POINTER(ShieldProfile), C.POINTER(ResourceState), C.c_int]
    resource, shield = ResourceProfile(), ShieldProfile()
    assert lib.resource_load(C.byref(resource), str(WORK / 'staged/BARGERES.DAT').encode()) == 1
    assert lib.shield_load(C.byref(shield), str(WORK / 'staged/BARGESH.DAT').encode()) == 1
    hull_max, hull_min = [float(v) for v in (WORK / 'BARGEHP.txt').read_text().split()[1:]]
    fixtures = {
        'healthy': (shield.capacity, resource.capacity, 0., hull_max, 0),
        'shield_drain': (0., resource.capacity, 0., hull_max, 0),
        'energy_starved': (0., 0., 0., hull_max, 0),
        'overheat_recovery': (shield.capacity, resource.capacity, resource.max_heat + 1., hull_max, 1),
        'hull_disabled': (0., 100., 0., hull_min - 1., 0),
    }
    with (WORK / 'native.csv').open(newline='') as stream: rows = list(csv.DictReader(stream))
    errors = {name: 0. for name in ('shields', 'energy', 'heat')}
    count = 0
    previous = None
    for row in rows:
        name = row['case']
        if name != previous:
            shields, energy, heat, hull, overheated = fixtures[name]
            state = ResourceState(energy, heat, overheated)
            shield_state = ShieldState(shields, 0)
            previous = name
        disabled = int(hull < hull_min or state.overheated)
        assert disabled == int(row['prior_disabled']), (name, row['tick'], 'prior disabled')
        lib.shield_tick(C.byref(shield_state), C.byref(shield), C.byref(state), disabled)
        lib.resource_tick_disabled(C.byref(state), C.byref(resource), int(hull < hull_min))
        for field, actual in (('shields', shield_state.shields), ('energy', state.energy), ('heat', state.heat)):
            error = abs(actual - float(row[field]))
            errors[field] = max(errors[field], error)
            assert error <= 1e-8, (name, row['tick'], field, error)
        assert state.overheated == int(row['overheated']), (name, row['tick'], 'overheated')
        assert shield_state.delay == int(row['delay']), (name, row['tick'], 'delay')
        assert int(hull < hull_min or state.overheated) == int(row['disabled']), (name, row['tick'], 'disabled')
        count += 1
    assert count == 430
    report = {'rows': count, 'cases': list(fixtures), 'max_absolute_error': errors,
              'comparison': 'native Ship::DoGeneration vs generic DOS shield_tick then resource_tick_disabled'}
    (WORK / 'check.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))

if __name__ == '__main__': check()
