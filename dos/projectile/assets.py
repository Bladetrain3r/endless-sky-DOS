#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage the qualified Energy Blaster profile and original collision masks."""
import hashlib
import json
from pathlib import Path
import shutil
ROOT=Path(__file__).resolve().parents[2]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def stage(out):
    work=ROOT/'.work/projectile'
    provenance=json.loads((work/'native-provenance.json').read_text())
    for file,key in [('native.csv','csv_sha256')]:
        assert digest(work/file)==provenance[key], 'Rebuild projectile native oracle'
    for file,key in [('dos/projectile/oracle.cpp','oracle_source_sha256'),
                     ('dos/projectile/native.py','runner_source_sha256'),
                     ('data/human/weapons.txt','weapon_data_sha256')]:
        assert digest(ROOT/file)==provenance[key], 'Rebuild projectile native oracle'
    for file,expected in provenance['behavior_source_sha256'].items():
        assert digest(ROOT/'source'/file)==expected, 'Rebuild native behavioral reference'
    w=provenance['weapon']
    for key in ('acceleration','drag','homing','random_velocity','random_lifetime','fade_out','turn','live_effects','die_effects','submunitions'):
        assert w[key]==0, (key,w[key])
    assert w['velocity']==10.625 and w['lifetime']==48 and w['reload']==12 and w['inaccuracy']==3
    out.mkdir(parents=True,exist_ok=True)
    (out/'BLASTER.DAT').write_text('ESBOLT1\n'+f"{w['velocity']:.17g} {int(w['lifetime'])} {int(w['reload'])} {w['inaccuracy']:.17g}\n")
    mask=ROOT/'.work/collision/run/MASKS.BIN'
    collision=json.loads((ROOT/'dos/reports/collision-equivalence.json').read_text())
    assert digest(mask)==collision['masks_sha256'], 'Rebuild qualified collision masks'
    shutil.copyfile(mask,out/'MASKS.BIN')
    report={'weapon':'Energy Blaster','stats':w,'native_csv_sha256':provenance['csv_sha256'],
            'profile_sha256':digest(out/'BLASTER.DAT'),'masks_sha256':digest(out/'MASKS.BIN'),
            'scope':'One training gun, unlimited resources, synthetic centreline muzzle; stock Sparrow beam loadout unchanged in reference.',
            'randomness':'Fixed LCG uniform pair, triangular spread; not native RNG sequence parity.'}
    (out/'blaster.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__': stage(ROOT/'.work/flight/run')
