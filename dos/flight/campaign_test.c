/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "campaign.h"
#include "../pursuit/pursuit.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char col)
{(void)f;(void)x;(void)y;(void)s;(void)col;}
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *b,const unsigned char *a)
{(void)f;(void)s;(void)h;(void)x;(void)y;(void)b;(void)a;}
static Campaign c,loaded;
static Navigation nav;
static Pilot pilot;
static Scene scene;
static Practice practice;
static Threat threat;
static PropulsionProfile drive;
static void keys(unsigned raw)
{
 unsigned k=campaign_keys(&c,&nav,&practice,&threat,raw,pilot.previous_keys);
 navigation_keys(&nav,&pilot,&scene,k);pilot_keys(&pilot,&scene,k);
}
static void tick(void)
{
 NavPhase old=nav.phase;
 navigation_step(&nav,&pilot,&scene,&practice,&threat,&drive);
 if(old!=NAV_DOCKED && nav.phase==NAV_DOCKED)campaign_landed(&c,&nav,&practice,&threat);
}
int main(void)
{
 unsigned i;uint64_t gen;PSState before;
 scene.earth_x=550;scene.earth_y=345;scene.luna_x=850;scene.luna_y=150;
 CHECK(navigation_load(&nav,&scene,"NAV.DAT"));
 CHECK(pilot_load(&pilot,"PILOT.DAT") && pursuit_load(&pilot.parameters,"APPROACH.DAT"));
 CHECK(practice_load(&practice,"BLASTER.DAT","MASKS.BIN"));
 CHECK(practice_damage_load(&practice,"DAMAGE.DAT") && practice_shields_load(&practice,"SHIELD.DAT"));
 CHECK(practice_resources_load(&practice,"RESOURCE.DAT",0));
 CHECK(threat_load(&threat,"PLAYER.DAT") && propulsion_load(&drive,"PROPULSE.DAT"));
 practice_reset(&practice);threat_reset(&threat);pilot_reset(&pilot,&scene);
 CHECK(campaign_open(&c,"SAVE",&nav,&pilot,&scene,&practice,&threat)==PS_OK);
 CHECK(c.session.generation==1 && nav.phase==NAV_DOCKED && nav.selected==0);
 gen=c.session.generation;keys(INPUT_RESET|INPUT_FIRE|INPUT_SHIELD_TEST);keys(0);
 CHECK(nav.phase==NAV_DOCKED && c.session.generation==gen && pilot.previous_keys==0);
 CHECK(!pilot.command.forward && !practice.shield_test_pulses);
 /* Native after-purchase fixture; trade UI is deliberately a later ticket. */
 c.state.credits=8485;c.state.cargo_count=1;
 strcpy(c.state.cargo[0].key,"commodity:Food");c.state.cargo[0].tons=3;c.state.cargo[0].basis=1515;
 keys(INPUT_SAVE);keys(INPUT_SAVE);keys(0);CHECK(c.session.generation==gen+1);
 before=c.state;
 practice.resources.energy=10.;practice.resources.heat=400.;practice.shield.shields=50.;threat.hull=200.;
 CHECK(campaign_open(&loaded,"SAVE",&nav,&pilot,&scene,&practice,&threat)==PS_OK);
 CHECK(!memcmp(&before,&loaded.state,sizeof(before)));
 CHECK(practice.resources.energy==4000. && practice.shield.shields==1400. && threat.hull==300.);
 CHECK(fabs(pilot.parameters.acceleration-.11507936507936507)<1e-16);
 CHECK(practice.resource_profile.max_heat==18900. && practice.resources.heat==nav.idle_heat);
 c=loaded;
 /* Failed pre-departure save cannot launch on a subsequent held-L poll. */
 strcpy(c.directory,"MISSING");keys(INPUT_LAND);keys(INPUT_LAND);
 CHECK(nav.phase==NAV_DOCKED && c.block_depart && c.dirty);
 strcpy(c.directory,"SAVE");keys(0);keys(INPUT_LAND);keys(0);
 CHECK(nav.phase==NAV_LAUNCHING);
 gen=c.session.generation;before=c.state;keys(INPUT_SAVE);keys(0);
 CHECK(c.session.generation==gen && !memcmp(&before,&c.state,sizeof(before)) && c.rejections==1);
 for(i=0;i<100 && nav.phase!=NAV_FLYING;++i)tick();
 CHECK(nav.phase==NAV_FLYING);
 keys(INPUT_PLANET);keys(0);keys(INPUT_LAND);keys(0);
 for(i=0;i<3600 && nav.phase!=NAV_DOCKED;++i)tick();
 CHECK(i<3600 && nav.selected==1 && !c.dirty);
 CHECK(!strcmp(c.state.planet,"planet:Luna") && c.state.credits==8485 && c.state.cargo[0].tons==3);
 CHECK(campaign_open(&loaded,"SAVE",&nav,&pilot,&scene,&practice,&threat)==PS_OK);
 CHECK(nav.selected==1 && nav.phase==NAV_DOCKED && loaded.state.cargo[0].basis==1515);
 CHECK(!memcmp(c.state.pilot_id,loaded.state.pilot_id,16) && !memcmp(c.state.ship_id,loaded.state.ship_id,16));
 gen=c.session.generation;strcpy(c.directory,"MISSING");
 campaign_landed(&c,&nav,&practice,&threat);
 CHECK(c.dirty && c.session.generation==gen);
 strcpy(c.directory,"SAVE");CHECK(campaign_save(&c,&nav,&practice,&threat)==PS_OK);
 CHECK(!c.dirty);
 printf("status=pass\ncampaign_port_reload=pass\ncargo_profile=pass\nheld_save_and_departure=pass\n");
 return 0;
}
