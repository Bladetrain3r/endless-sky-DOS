/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "threat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char color)
{ (void)f;(void)x;(void)y;(void)s;(void)color; }
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *blend,const unsigned char *add)
{ (void)f;(void)s;(void)h;(void)x;(void)y;(void)blend;(void)add; }
static void tick(Threat *t,Practice *p,Pilot *pilot)
{
    practice_finish_tick(p,pilot,0);
    threat_step(t,p,pilot);
}
static void fixture(Threat *t,Practice *p,Pilot *pilot)
{
    Scene scene={0};
    practice_reset(p);threat_reset(t);pilot_reset(pilot,&scene);
    p->moving_target=0;
    pilot->state.y=230.;
    t->ticks=THREAT_GRACE_TICKS-1;
}
static void until_hit(Threat *t,Practice *p,Pilot *pilot)
{
    unsigned i;
    for(i=0;i<40 && !t->hits;++i) tick(t,p,pilot);
    CHECK(t->hits==1);
}
int main(void)
{
    static Practice p;
    Threat t={0},saved;
    Pilot pilot={0};
    unsigned i;
    double expected;
    CHECK(practice_load(&p,"BLASTER.DAT","MASKS.BIN"));
    CHECK(practice_damage_load(&p,"DAMAGE.DAT"));
    CHECK(practice_shields_load(&p,"SHIELD.DAT"));
    CHECK(threat_load(&t,"PLAYER.DAT"));
    CHECK(t.enabled && t.max_hull==300. && t.minimum_hull==134. && t.hull==300.);
    saved=t;
    CHECK(!threat_load(&t,"BAD.DAT"));
    CHECK(t.hull==saved.hull && t.enabled==saved.enabled);
    practice_reset(&p);threat_reset(&t);
    p.moving_target=0;
    for(i=0;i<THREAT_GRACE_TICKS-1;++i) tick(&t,&p,&pilot);
    CHECK(t.ticks==THREAT_GRACE_TICKS-1 && !t.shots && !t.active);
    p.destroyed=1;
    for(i=0;i<2u*(unsigned)p.reload;++i) tick(&t,&p,&pilot);
    CHECK(!t.shots);
    fixture(&t,&p,&pilot);
    tick(&t,&p,&pilot);
    CHECK(t.ticks==120 && t.shots==1 && t.cooldown==(unsigned)p.reload);
    CHECK(t.bolts[0].alive && t.bolts[0].angle==motion_angle(180.));
    until_hit(&t,&p,&pilot);
    CHECK(t.hull==300. && fabs(p.shield.shields-(p.shield_profile.capacity-p.shield_damage))<1e-10);
    CHECK(!t.disabled && !t.destroyed && t.flash>0);
    /* The shield helper must set delay without taking the same damage twice. */
    CHECK(p.shield.delay==p.shield_profile.delay);
    fixture(&t,&p,&pilot);
    p.shield.shields=5.;
    until_hit(&t,&p,&pilot);
    expected=300.-p.hull_damage*(1.-5./p.shield_damage);
    CHECK(p.shield.shields==0. && fabs(t.hull-expected)<1e-10);
    CHECK(p.shield.delay==p.shield_profile.depleted_delay);
    /* A shot keeps its fixed launch direction when the player dodges. */
    fixture(&t,&p,&pilot);
    tick(&t,&p,&pilot);
    pilot.state.x+=300.;
    for(i=0;i<20;++i) tick(&t,&p,&pilot);
    CHECK(!t.hits && p.shield.shields==p.shield_profile.capacity);
    /* The turret uses the CURRENT player pose: no velocity lead or tracking. */
    fixture(&t,&p,&pilot);
    pilot.state.vx=80.;
    tick(&t,&p,&pilot);
    CHECK(t.bolts[0].angle==motion_angle(180.));
    CHECK(fabs(t.bolts[0].vx)<1e-8);
    /* The moving ellipse contributes one observed target-position delta. */
    fixture(&t,&p,&pilot);
    p.moving_target=1;t.ticks=THREAT_GRACE_TICKS-2;
    tick(&t,&p,&pilot);
    {
        double x=p.target.x,y=p.target.y,ux,uy,vx,vy;
        tick(&t,&p,&pilot);
        vx=p.target.x-x;vy=p.target.y-y;
        motion_unit(t.bolts[0].angle,&ux,&uy);
        CHECK(t.shots==1 && fabs(t.bolts[0].vx-(vx+p.speed*ux))<1e-10);
        CHECK(fabs(t.bolts[0].vy-(vy+p.speed*uy))<1e-10);
        CHECK(fabs(t.bolts[0].x-(p.target.x+20.*ux-.5*vx))<1e-10);
    }
    /* A dead source cannot fire again, but its shot remains lethal. */
    fixture(&t,&p,&pilot);
    p.shield.shields=0.;t.hull=1.;
    tick(&t,&p,&pilot);
    CHECK(t.shots==1 && t.bolts[0].alive);
    p.destroyed=1;
    until_hit(&t,&p,&pilot);
    CHECK(t.destroyed && t.disabled && t.hull<0. && t.shots==1);
    saved=t;
    bolt_launch(&t.bolts[0],pilot.state.x,pilot.state.y,0.,0.,0,p.speed,p.lifetime);
    tick(&t,&p,&pilot);
    CHECK(t.hits==saved.hits && t.hull==saved.hull && t.shots==saved.shots);
    /* Both disable and death comparisons are strict. */
    fixture(&t,&p,&pilot);
    p.shield.shields=0.;
    t.hull=t.minimum_hull+p.hull_damage;
    until_hit(&t,&p,&pilot);
    CHECK(t.hull==t.minimum_hull && !t.disabled && !t.destroyed);
    fixture(&t,&p,&pilot);
    p.shield.shields=0.;t.hull=p.hull_damage;
    until_hit(&t,&p,&pilot);
    CHECK(t.hull==0. && t.disabled && !t.destroyed);
    fixture(&t,&p,&pilot);
    p.shield.shields=0.;t.hull=t.minimum_hull;
    until_hit(&t,&p,&pilot);
    CHECK(t.disabled && !t.destroyed);
    fixture(&t,&p,&pilot);
    CHECK(t.hull==t.max_hull && !t.disabled && !t.destroyed && !t.shots && !t.hits);
    CHECK(!t.bolts[0].alive && !t.have_previous_target && p.shield.shields==p.shield_profile.capacity);
    puts("status=pass\nprofile=300_134\ngraced_direct_hit=pass\nshield_overflow_delay=pass\n"
         "dodge_and_no_ai=pass\nsource_velocity=pass\nsource_death_retains_shot=pass\ndeath_ignores_later_hits=pass\n"
         "disable_threshold_and_reset=pass");
    return 0;
}
