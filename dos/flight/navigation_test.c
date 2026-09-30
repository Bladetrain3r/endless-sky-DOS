/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "navigation.h"
#include "../pursuit/pursuit.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char color)
{ (void)f;(void)x;(void)y;(void)s;(void)color; }
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *b,const unsigned char *a)
{ (void)f;(void)s;(void)h;(void)x;(void)y;(void)b;(void)a; }
static Navigation nav;
static Practice practice;
static Pilot pilot;
static Threat player;
static PropulsionProfile drive;
static Scene scene;
static void keys(unsigned value)
{
    navigation_keys(&nav,&pilot,&scene,value);pilot_keys(&pilot,&scene,value);
}
static void tick(void) {navigation_step(&nav,&pilot,&scene,&practice,&player,&drive);}
static void reset(void)
{
    navigation_reset(&nav);practice_reset(&practice);threat_reset(&player);
    pilot_reset(&pilot,&scene);pilot.previous_keys=0;pilot.ticks=0;
}
int main(void)
{
    unsigned i,earth_ticks,luna_ticks;
    NavDestination *d;
    scene.earth_x=550;scene.earth_y=345;scene.luna_x=850;scene.luna_y=150;
    CHECK(navigation_load(&nav,&scene,"NAV.DAT"));
    CHECK(pilot_load(&pilot,"PILOT.DAT") && pursuit_load(&pilot.parameters,"APPROACH.DAT"));
    CHECK(practice_load(&practice,"BLASTER.DAT","MASKS.BIN"));
    CHECK(practice_damage_load(&practice,"DAMAGE.DAT") && practice_shields_load(&practice,"SHIELD.DAT"));
    CHECK(practice_resources_load(&practice,"RESOURCE.DAT",0));
    CHECK(threat_load(&player,"PLAYER.DAT") && propulsion_load(&drive,"PROPULSE.DAT"));
    reset();d=&nav.destinations[0];
    /* Native strict geometry and state gates; denied content does not start
     * an approach and cannot be bypassed by being centred on the planet. */
    pilot.state=(MotionState){d->x,d->y,0.,0.,0};
    CHECK(navigation_can_land(&nav,&pilot,&practice,&player));
    pilot.state.x=d->x+d->radius;CHECK(!navigation_can_land(&nav,&pilot,&practice,&player));
    pilot.state.x=d->x+d->radius-.00001;CHECK(navigation_can_land(&nav,&pilot,&practice,&player));
    pilot.state.vx=1.;CHECK(!navigation_can_land(&nav,&pilot,&practice,&player));
    pilot.state.vx=.99999;CHECK(navigation_can_land(&nav,&pilot,&practice,&player));
    player.disabled=1;CHECK(!navigation_can_land(&nav,&pilot,&practice,&player));player.disabled=0;
    practice.resources.overheated=1;CHECK(!navigation_can_land(&nav,&pilot,&practice,&player));
    practice.resources.overheated=0;
    d->flags&=~NAV_ALLOWED;keys(INPUT_LAND);CHECK(!nav.approach && !navigation_can_land(&nav,&pilot,&practice,&player));
    d->flags|=NAV_ALLOWED;keys(0);
    /* Autopilot cancels under manual steering, destination change, or L toggle. */
    reset();keys(INPUT_LAND);CHECK(nav.approach);keys(INPUT_LEFT);CHECK(!nav.approach);
    keys(INPUT_LAND);keys(INPUT_PLANET);CHECK(!nav.approach && nav.selected==1);
    keys(INPUT_LAND);keys(0);keys(INPUT_LAND);CHECK(!nav.approach && nav.cancellations==3);
    /* Real approach -> dock at each destination; holding L through docking
     * does not depart, and available port services restore native fields. */
    reset();practice_shield_test(&practice);player.hull=200.;practice.resources.energy=100.;
    keys(INPUT_LAND);
    for(i=0;i<3600 && nav.phase!=NAV_DOCKED;++i) {tick();keys(INPUT_LAND);}
    earth_ticks=i;CHECK(i<3600 && nav.landings==1 && !nav.launches);
    CHECK(practice.shield.shields==1400. && player.hull==300. && practice.resources.energy==4000.);
    CHECK(practice.resources.heat==nav.idle_heat && !practice.active && !practice.cooldown);
    {
        Practice before=practice;Pilot p=pilot;unsigned ticks=nav.ticks;
        for(i=0;i<300;++i) {tick();keys(INPUT_LAND);}
        CHECK(nav.phase==NAV_DOCKED && nav.ticks==ticks && !memcmp(&before,&practice,sizeof(before)));
        CHECK(!memcmp(&p,&pilot,sizeof(p)));
    }
    /* Departure is a fresh key edge, outward speed1 within radius, then zoom
     * back to normal. Keys held in dock/launch must be released before acting. */
    keys(0);keys(INPUT_LAND|INPUT_FORWARD|INPUT_FIRE);
    CHECK(nav.phase==NAV_LAUNCHING && nav.launches==1 && nav.zoom==0.f);
    CHECK(hypot(pilot.state.x-d->x,pilot.state.y-d->y)<d->radius);
    CHECK(fabs(hypot(pilot.state.vx,pilot.state.vy)-1.)<1e-12);
    for(i=0;i<100;++i) {tick();keys(INPUT_FORWARD|INPUT_FIRE);}
    CHECK(nav.phase==NAV_FLYING && nav.zoom==1.f && !practice.shots);
    CHECK(fabs(hypot(pilot.state.vx,pilot.state.vy)-1.)<1e-12);
    keys(0);tick();keys(INPUT_FIRE);tick();CHECK(practice.shots==1);
    keys(INPUT_PLANET);CHECK(nav.selected==1);keys(INPUT_LAND);keys(0);
    for(i=0;i<3600 && nav.phase!=NAV_DOCKED;++i) tick();
    luna_ticks=i;CHECK(i<3600 && nav.landings==2 && nav.selected==1);
    /* R-equivalent reset restores encounter counters and all caller-owned state. */
    reset();CHECK(nav.phase==NAV_FLYING && !nav.landings && !nav.launches && !practice.shots);
    CHECK(pilot.state.x==400. && pilot.state.y==300. && nav.zoom==1.f);
    /* Fade suppresses firing; a newly disabled ship aborts the descent rather
     * than obtaining free repairs by finishing a failed landing. */
    pilot.state.x=d->x;pilot.state.y=d->y;keys(INPUT_LAND);tick();CHECK(nav.phase==NAV_LANDING);
    keys(INPUT_FIRE);tick();CHECK(!practice.shots && nav.zoom<1.f);
    player.disabled=1;tick();CHECK(nav.phase==NAV_FLYING && !nav.landings && nav.cancellations==1);
    /* Invalid profile loads leave the live navigation state intact. */
    {
        Navigation before=nav;FILE *f=fopen("BADNAV.DAT","wb");CHECK(f);
        fputs("ESNAV1",f);fclose(f);
        CHECK(!navigation_load(&nav,&scene,"BADNAV.DAT") && !memcmp(&nav,&before,sizeof(nav)));
    }
    printf("status=pass\nstrict_landing_gates=pass\napproach_cancel=pass\nport_services=pass\n"
        "docked_freeze_and_held_key=pass\nlaunch_and_release_gate=pass\nreset_and_abort=pass\n"
        "transactional_profile=pass\nearth_approach_ticks=%u\nluna_approach_ticks=%u\n",earth_ticks,luna_ticks);
    return 0;
}
