#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual staged flight application through DOSBox's keyboard."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
RUN = ROOT / '.work/flight/run'
IMAGE = 'modern-arena-tools:0.1'


def guest(stress=None,incoming=False,pursuit=False,arena=False,navigation=False,launch=False):
    result = RUN / 'KEYS.TXT'
    result.unlink(missing_ok=True)
    process = subprocess.Popen(['dosbox', '-conf', '.work/flight/run/keys.conf'],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 25
        window = None
        while time.monotonic() < deadline:
            assert process.poll() is None, 'DOSBox exited before the flight view'
            found = subprocess.run(['xdotool', 'search', '--name', 'DOSBox'], capture_output=True, text=True)
            for candidate in found.stdout.split():
                geometry = subprocess.run(['xdotool', 'getwindowgeometry', '--shell', candidate],
                                          capture_output=True, text=True).stdout
                if 'WIDTH=800\n' in geometry and 'HEIGHT=600\n' in geometry:
                    window = candidate
                    break
            if window: break
            time.sleep(.1)
        assert window, '800x600 flight window did not appear'
        subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True)
        time.sleep(.3)

        def send(action, key, pause):
            subprocess.run(['xdotool', action, key], check=True)
            time.sleep(pause)

        if navigation:
            send('key','p',.1);send('key','p',.1)
            send('key','l',.2);send('keydown','w',.2);send('keyup','w',.1)
            send('key','r',.2)
            send('keydown','h',.8);send('keyup','h',.1)
            # Holding L crosses the dock transition: it must not auto-depart.
            send('keydown','l',6.);send('keyup','l',.2)
            if launch:
                send('key','l',1.5)
                send('keydown','space',.4);send('keyup','space',.2)
                send('key','p',.1);send('key','l',.2);send('key','l',.2)
            send('key','Escape',.1)
        elif arena:
            # Exercise movement/fire and camera, then reset. Only the post-reset
            # target taps may remain in selection counters. Hold T past repeat.
            send('keydown', 'w', .3)
            send('keydown', 'd', .4)
            send('keyup', 'w', .1)
            send('keyup', 'd', .1)
            send('keydown', 'space', .5)
            send('keyup', 'space', .1)
            send('key', 'Tab', .1)
            send('key', 'r', .2)
            send('keydown', 't', .8)
            send('keyup', 't', .1)
            send('key', 'n', .1)
            # Reset again to guarantee nearest target and grace start regardless
            # of scheduling; final held T must produce exactly one change.
            send('key', 'r', .2)
            send('keydown', 't', .8)
            send('keyup', 't', 3.2)
            send('key', 'Tab', .1)
            send('key', 'Escape', .1)
        elif incoming or pursuit:
            send('keydown', 'h', .8)
            send('keyup', 'h', 4.0)
            send('key', 'Escape', .1)
        else:
            send('keydown', 'w', .3)
            send('keydown', 'd', .5)
            send('keyup', 'w', .2)
            send('keyup', 'd', .2)
            send('key', 'Tab', .2)
            send('key', 'r', .2)
            send('keydown', 'space', 8. if stress else .8)
            send('keydown', 'w', .5)
            send('keyup', 'w', .2)
            send('keyup', 'space', 3.2 if stress else 1.0)
            if stress:
                # Engines must work again after cooling/recharge. Then allow the
                # small stress battery to refill before checking recovery below.
                send('keydown', 'w', .3)
                send('keyup', 'w', 3.2)
            send('key', 'Tab', .2)
            # One held trainer key is one drain, not one drain per frame/repeat.
            send('keydown', 'h', .8)
            send('keyup', 'h', .8)
            send('key', 'Escape', .1)
        process.wait(timeout=20)
        fields = dict(line.split('=', 1) for line in result.read_text().splitlines() if '=' in line)
        assert fields['status'] == 'ok' and fields['video_verify'] == 'pass', fields
        if navigation:
            assert fields['navigation']=='1' and fields['nav_landings']=='1',fields
            assert fields['nav_launches']==('1' if launch else '0'),fields
            assert fields['nav_phase']==('0' if launch else '2'),fields
            assert fields['nav_approach']=='0' and fields['shield_test_pulses']=='1',fields
            assert fields['player_shields']=='1400' and fields['player_hull']=='300',fields
            assert int(fields['pilot_input_bits']) & (2048|4096)==(2048|4096),fields
            assert fields['enemy_shots']=='0' and float(fields['discarded_sim_ms'])==0.,fields
            assert int(fields['nav_docked_ticks'])>60,fields
            if launch:
                assert int(fields['practice_shots'])>=1 and fields['nav_selected']=='1',fields
                assert fields['nav_cancellations']=='1',fields
            else:
                assert fields['energy']=='4000' and fields['practice_shots']=='0',fields
                assert fields['nav_selected']=='0' and fields['nav_zoom']=='0',fields
            print(json.dumps(fields))
            return
        if arena:
            assert fields['arena']=='1' and fields['arena_alive']=='2',fields
            assert fields['arena_selected']=='1' and fields['arena_selection_changes']=='1',fields
            assert fields['practice_shots']=='0' and fields['pilot_follow']=='0',fields
            assert float(fields['pilot_x'])==400. and float(fields['pilot_y'])==300.,fields
            assert int(fields['pilot_input_bits']) & (512|1024)==(512|1024),fields
            assert int(fields['arena_0_shots'])>0 and int(fields['arena_1_shots'])>0,fields
            assert int(fields['enemy_hits'])>0 and fields['enemy_dropped']=='0',fields
            assert fields['discarded_sim_ms']=='0.000',fields
            print(json.dumps(fields))
            return
        if incoming or pursuit:
            assert fields['incoming_fire']=='1' and int(fields['enemy_hits'])>0,fields
            assert fields['shield_test_pulses']=='1' and fields['practice_shots']=='0',fields
            assert float(fields['player_shields'])<1050. and float(fields['player_hull'])==300.,fields
            assert fields['enemy_dropped']=='0' and fields['discarded_sim_ms']=='0.000',fields
            if pursuit:
                assert fields['pursuit']=='1' and int(fields['opponent_turn_ticks'])>0,fields
                assert int(fields['opponent_thrust_ticks'])>0,fields
                assert float(fields['opponent_x'])!=650. or float(fields['opponent_y'])!=140.,fields
            print(json.dumps(fields))
            return
        assert int(fields['pilot_input_bits']) == 505 and fields['pilot_keyboard'] == '1', fields
        # About 1.5s held fire at5Hz, then >1s released. Broad scheduling
        # allowance still rejects a stuck fire bit after release.
        if stress:
            assert 7 <= int(fields['practice_shots']) < 40, fields
            assert int(fields['blocked_'+stress+'_ticks']) > 0, fields
            assert fields['overheated'] == '0', fields
            if stress == 'energy': assert float(fields['energy']) >= 40., fields
            else: assert float(fields['heat']) < 405., fields
        else:
            assert 7 <= int(fields['practice_shots']) <= 10, fields
        assert fields['practice_dropped'] == '0', fields
        assert int(fields['shield_test_pulses']) == 1, fields
        shield, capacity = float(fields['player_shields']), float(fields['player_shield_capacity'])
        assert .75 * capacity < shield < capacity, fields
        assert fields['target_destroyed'] == '1' and int(fields['practice_hits']) == 7, fields
        assert float(fields['target_hull']) < 0 and float(fields['target_shields']) == 0, fields
        assert fields['pilot_follow'] == '0' and fields['pilot_angle'] == '0', fields
        assert abs(float(fields['pilot_x']) - 400.) < 1e-9 and float(fields['pilot_y']) < 300., fields
        assert float(fields['pilot_vy']) < 0. and fields['discarded_sim_ms'] == '0.000', fields
        print(json.dumps(fields))
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=3)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--navigation',action='store_true')
    parser.add_argument('--launch',action='store_true',help='also leave dock and check flight controls')
    parser.add_argument('--arena',action='store_true')
    parser.add_argument('--guest',action='store_true')
    parser.add_argument('--incoming-fire',action='store_true')
    parser.add_argument('--pursuit',action='store_true')
    parser.add_argument('--resource-stress',choices=('energy','heat'))
    args = parser.parse_args()
    if args.launch and not args.navigation: parser.error('launch requires navigation')
    if args.navigation and (args.arena or args.pursuit or args.incoming_fire or args.resource_stress): parser.error('navigation is a separate test')
    if args.guest: return guest(args.resource_stress,args.incoming_fire,args.pursuit,args.arena,args.navigation,args.launch)
    stress_option = ' --resource-stress '+args.resource_stress if args.resource_stress else ''
    if args.incoming_fire: stress_option+=' --incoming-fire'
    if args.navigation: stress_option+=' --navigation'
    if args.arena: stress_option+=' --arena'
    if args.pursuit: stress_option+=' --pursuit'
    target_option='' if (args.pursuit or args.arena or args.navigation) else ' --stationary-target'
    base = (ROOT / 'dos/probes/runtime.conf').read_text().split('[autoexec]')[0]
    config = base + ('[autoexec]\nmount c /work/.work/flight/run\nc:\nCWSDPMI -s-\n'
                     f'FLIGHT.EXE --demo --pilot{target_option} --seconds 25{stress_option} > KEYS.TXT\nexit\n')
    (RUN / 'keys.conf').write_text(config)
    tested_hash = hashlib.sha256((RUN / 'FLIGHT.EXE').read_bytes()).hexdigest()
    result = subprocess.run(['docker', 'run', '--rm', '--network', 'none', '--user',
        f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp', '-v', f'{ROOT}:/work', '-w', '/work',
        IMAGE, 'timeout', '55s', 'xvfb-run', '-a', 'python3', 'dos/flight/test_controls.py', '--guest',
        *(['--resource-stress',args.resource_stress] if args.resource_stress else []),
        *(['--incoming-fire'] if args.incoming_fire else []),
        *(['--pursuit'] if args.pursuit else []),
        *(['--arena'] if args.arena else []),
        *(['--navigation'] if args.navigation else []),
        *(['--launch'] if args.launch else [])],
        check=True, capture_output=True, text=True, timeout=65)
    fields = json.loads(result.stdout)
    assert tested_hash == hashlib.sha256((RUN / 'FLIGHT.EXE').read_bytes()).hexdigest()
    report = {'navigation_profiles':{name:hashlib.sha256((RUN/name).read_bytes()).hexdigest() for name in ('NAV.DAT','APPROACH.DAT') if args.navigation}, 'result': fields, 'exe_sha256': tested_hash, 'resource_stress': args.resource_stress,
              'opponent_profile_sha256':{name:hashlib.sha256((RUN/name).read_bytes()).hexdigest() for name in ('BARGERES.DAT','BARGESH.DAT','BARGEPRO.DAT','BARGEHP.DAT')},
            'pursuit_profile_sha256': hashlib.sha256((RUN/'PURSUIT.DAT').read_bytes()).hexdigest(),
              'player_profile_sha256': hashlib.sha256((RUN/'PLAYER.DAT').read_bytes()).hexdigest(),
              'resource_profile_sha256': hashlib.sha256((RUN/'RESOURCE.DAT').read_bytes()).hexdigest(),
              'shield_profile_sha256': hashlib.sha256((RUN/'SHIELD.DAT').read_bytes()).hexdigest(),
              'propulsion_profile_sha256': hashlib.sha256((RUN/'PROPULSE.DAT').read_bytes()).hexdigest(), 'dos_config': config,
              'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'image_id': subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',IMAGE],text=True).strip(),
              'scope': ('Navigation: target cycle, manual cancel/reset, held L across docking, native port restoration and paused sim; optional takeoff/fire/approach toggle. ' if args.navigation else '')+('Arena: pre-reset thrust/turn/fire, reset, held T cycle once, N delivery, camera, two owners firing at one player; ' if args.arena else '')+'X11 key injection into actual pilot build: simultaneous thrust/turn, releases, camera/reset/Escape and held Space shots/hits; held H single shield drain and recharge; physical feel awaits human check.'}
    (ROOT / ('dos/reports/flight-controls'+('-navigation-launch' if args.navigation and args.launch else '-navigation-dock' if args.navigation else '-arena' if args.arena else '-pursuit' if args.pursuit else '-incoming' if args.incoming_fire else '')+('-'+args.resource_stress if args.resource_stress else '')+'.json')).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(fields, indent=2))


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(f'{exc}\n{exc.stdout or ""}\n{exc.stderr or ""}')
