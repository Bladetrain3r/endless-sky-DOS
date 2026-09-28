/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "power.h"
#include "practice.h"
#include "input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char color)
{ (void)f;(void)x;(void)y;(void)s;(void)color; }
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *blend,const unsigned char *add)
{ (void)f;(void)s;(void)h;(void)x;(void)y;(void)blend;(void)add; }
static void tick(Practice *p,Pilot *pilot,Scene *scene,const PropulsionProfile *drive,int fire)
{
    practice_begin_tick(p);
    pilot_powered_step(pilot,scene,&p->resources,&p->resource_profile,drive);
    practice_finish_tick(p,pilot,fire);
}
int main(void)
{
    static Practice p;
    Pilot pilot={0};Scene scene={0};PropulsionProfile drive;unsigned i,recovered=0;
    CHECK(pilot_load(&pilot,"PILOT.DAT") && propulsion_load(&drive,"PROPULSE.DAT"));
    CHECK(practice_load(&p,"BLASTER.DAT","MASKS.BIN") && practice_resources_load(&p,"RESOURCE.DAT",0));
    pilot_reset(&pilot,&scene);
    /* Exactly enough for either thrust plus a shortfall, or the gun alone:
     * the real app must generate once, move first, and deny the gun. */
    p.resources.energy=p.resource_profile.shot_energy+drive.thrust_energy-.1-p.resource_profile.generation;
    pilot_keys(&pilot,&scene,INPUT_FORWARD|INPUT_FIRE);
    tick(&p,&pilot,&scene,&drive,1);
    CHECK(pilot.state.vy<0. && !p.shots && p.blocked_energy==1 && !p.cooldown && p.rng==17);
    CHECK(fabs(p.resources.energy-(p.resource_profile.shot_energy-.1))<1e-10);
    CHECK(fabs(p.resources.heat-p.resource_profile.heat_generation-drive.thrust_heat)<1e-10);
    CHECK(pilot.ticks==1 && (int)lround(pilot.state.y)-scene.camera_y==300);
    /* Under .25/tick supply, steering takes its native fractional share first,
     * leaving no thrust or weapon energy. No reverse-engine charge on BACK. */
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",1));practice_reset(&p);pilot_reset(&pilot,&scene);
    p.resources.energy=0.;pilot_keys(&pilot,&scene,INPUT_FORWARD|INPUT_RIGHT|INPUT_FIRE);
    tick(&p,&pilot,&scene,&drive,1);
    CHECK(pilot.state.angle==motion_angle(pilot.parameters.turn_rate*.25/drive.turn_energy));
    CHECK(pilot.state.vx==0. && pilot.state.vy==0. && p.resources.energy==0. && !p.shots);
    pilot_keys(&pilot,&scene,INPUT_BACK);tick(&p,&pilot,&scene,&drive,0);
    CHECK(p.resources.energy==.25 && pilot.state.vx==0. && pilot.state.vy==0.);
    /* Overheat must affect movement and firing in the same tick. Momentum
     * decays under native disabled drag; heading stays fixed; camera follows. */
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",2));practice_reset(&p);pilot_reset(&pilot,&scene);
    p.resources.heat=p.resource_profile.max_heat*1.01/(1.-p.resource_profile.dissipation);
    pilot.state.vx=2.;pilot.state.vy=-3.;pilot_keys(&pilot,&scene,INPUT_FORWARD|INPUT_RIGHT|INPUT_FIRE);
    tick(&p,&pilot,&scene,&drive,1);
    CHECK(p.resources.overheated && !pilot.state.angle && !p.shots && p.blocked_heat==1);
    CHECK(fabs(pilot.state.vx-2.*(1.-pilot.parameters.drag))<1e-12);
    CHECK(fabs(pilot.state.vy+3.*(1.-pilot.parameters.drag))<1e-12);
    CHECK((int)lround(pilot.state.x)-scene.camera_x==400);
    CHECK((int)lround(pilot.state.y)-scene.camera_y==300);
    pilot_keys(&pilot,&scene,0);
    for(i=0;i<600;++i) {
        tick(&p,&pilot,&scene,&drive,0);
        if(!p.resources.overheated) { recovered=1; break; }
    }
    CHECK(recovered);
    {
        double vx=pilot.state.vx,vy=pilot.state.vy;
        tick(&p,&pilot,&scene,&drive,0);
        CHECK(pilot.state.vx==vx && pilot.state.vy==vy); /* Healthy coasting. */
        pilot_keys(&pilot,&scene,INPUT_FORWARD|INPUT_FIRE);
        tick(&p,&pilot,&scene,&drive,1);
        CHECK(p.shots==1 && pilot.state.vy<vy);
    }
    practice_reset(&p);pilot_keys(&pilot,&scene,INPUT_RESET);
    CHECK(!p.resources.overheated && !p.resources.heat && p.resources.energy==p.resource_profile.capacity);
    CHECK(!pilot.state.vx && !pilot.state.vy && pilot.state.x==400. && pilot.state.y==300.);
    /* Regeneration consumes the previous tick's energy, not this tick's
     * production. Exercise the real Practice composition with a loaded profile. */
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",0));
    CHECK(practice_shields_load(&p,"SHIELD.DAT"));
    practice_reset(&p);pilot_reset(&pilot,&scene);
    CHECK(p.shield.shields==p.shield_profile.capacity);
    practice_shield_test(&p);
    CHECK(p.shield.shields==.75*p.shield_profile.capacity && p.shield_test_pulses==1);
    p.resources.energy=0.;
    {
        double before=p.shield.shields;
        tick(&p,&pilot,&scene,&drive,0);
        CHECK(p.shield.shields==before && p.resources.energy==p.resource_profile.generation);
        tick(&p,&pilot,&scene,&drive,0);
        CHECK(p.shield.shields>before && p.resources.energy>=p.resource_profile.generation);
    }
    /* Same low-budget fixture as powered motion: repair uses old energy,
     * then steering takes newly generated power, leaving no thrust or gun. */
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",1));
    practice_reset(&p);practice_shield_test(&p);pilot_reset(&pilot,&scene);
    p.resources.energy=.1;pilot_keys(&pilot,&scene,INPUT_FORWARD|INPUT_RIGHT|INPUT_FIRE);
    {
        double before=p.shield.shields;
        tick(&p,&pilot,&scene,&drive,1);
        CHECK(p.shield.shields>before && p.resources.energy==0. && !p.shots);
        CHECK(pilot.state.vx==0. && pilot.state.vy==0.);
    }
    /* Prior overheating suppresses repair even on the tick that cools enough
     * to restore movement. Repair resumes on the following tick. */
    practice_reset(&p);practice_shield_test(&p);pilot_reset(&pilot,&scene);
    p.resources.overheated=1;p.resources.heat=0.;
    {
        double before=p.shield.shields;
        tick(&p,&pilot,&scene,&drive,0);
        CHECK(p.shield.shields==before && !p.resources.overheated);
        tick(&p,&pilot,&scene,&drive,0);
        CHECK(p.shield.shields>before);
    }
    for(i=0;i<5;++i) practice_shield_test(&p);
    CHECK(p.shield.shields==0.);
    practice_reset(&p);
    CHECK(p.shield.shields==p.shield_profile.capacity && !p.shield.delay && !p.shield_test_pulses);
    /* Persistent hull disable is distinct from temporary overheating: even
     * cool hull-disabled ships get no recharge, power generation or controls. */
    practice_reset(&p);practice_shield_test(&p);pilot_reset(&pilot,&scene);
    p.resources.energy=10.;pilot.state.vx=2.;pilot.state.vy=-3.;
    {
        double before=p.shield.shields;
        unsigned ticks_before=pilot.ticks;
        practice_begin_tick_disabled(&p,1);
        pilot_disabled_step(&pilot,&scene);
        practice_finish_tick(&p,&pilot,0);
        CHECK(p.shield.shields==before && p.resources.energy==10. && !p.resources.overheated);
        CHECK(fabs(pilot.state.vx-2.*(1.-pilot.parameters.drag))<1e-12);
        CHECK(fabs(pilot.state.vy+3.*(1.-pilot.parameters.drag))<1e-12);
        CHECK(!p.shots && pilot.ticks==ticks_before+1);
    }
    puts("hull_disabled_generation_and_drift=pass");
    puts("shield_composition=pass\nshield_reset_and_drain=pass");
    puts("status=pass\ngeneration_movement_gun_order=pass\npartial_turn_before_thrust=pass\nshared_overheat_recovery=pass\nreset_and_camera=pass");
    return 0;
}
