/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "opponent.h"
#include "threat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char color)
{ (void)f;(void)x;(void)y;(void)s;(void)color; }
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *b,const unsigned char *a)
{ (void)f;(void)s;(void)h;(void)x;(void)y;(void)b;(void)a; }
void target_power_tests(Practice *,Opponent *,Threat *,Pilot *);
int main(void)
{
    static Practice p;
    Pilot pilot={0};Opponent enemy;Threat threat;unsigned i;
    CHECK(practice_load(&p,"BLASTER.DAT","MASKS.BIN"));
    CHECK(practice_damage_load(&p,"DAMAGE.DAT") && practice_shields_load(&p,"SHIELD.DAT"));
    CHECK(opponent_load(&enemy,"PURSUIT.DAT") && threat_load(&threat,"PLAYER.DAT"));
    enemy.enabled=1;opponent_reset(&enemy,&p);
    CHECK(p.pursuit_target && p.target.x==650. && p.target.y==140.);
    /* A distant target ahead must be approached using bounded physical motion. */
    enemy.state=(MotionState){0};pilot.state=(MotionState){0.,-1000.,0.,0.,0};
    for(i=0;i<240;++i) {
        double x=enemy.state.x,y=enemy.state.y;
        opponent_step(&enemy,&p,&pilot,0);
        CHECK(fabs(enemy.state.x-x-enemy.state.vx)<1e-10);
        CHECK(fabs(enemy.state.y-y-enemy.state.vy)<1e-10);
        practice_finish_tick(&p,&pilot,0);
        CHECK(p.target.x==enemy.state.x && p.target.y==enemy.state.y && p.target.angle==enemy.state.angle);
    }
    CHECK(enemy.state.y< -100. && enemy.thrust_ticks>0 && enemy.turn_ticks==0);
    /* A target behind needs an actual turn; no position/facing teleport. */
    opponent_reset(&enemy,&p);enemy.state=(MotionState){0};pilot.state.y=1000.;
    opponent_step(&enemy,&p,&pilot,0);
    CHECK(enemy.state.x==0. && enemy.state.y==0. && enemy.turn_ticks==1 && !enemy.thrust_ticks);
    for(i=0;i<400;++i) {
        uint16_t before=enemy.state.angle;
        int delta;
        opponent_step(&enemy,&p,&pilot,0);
        delta=(int16_t)(enemy.state.angle-before);
        CHECK(abs(delta)<=(int)(enemy.parameters.turn_rate*65536./360.+1.));
    }
    CHECK(enemy.thrust_ticks>0 && enemy.state.y>0.);
    /* A dead opponent/player cannot keep receiving pursuit updates. */
    {
        unsigned ticks=enemy.ticks;double x=enemy.state.x,y=enemy.state.y;
        p.destroyed=1;opponent_step(&enemy,&p,&pilot,0);
        CHECK(enemy.ticks==ticks && enemy.state.x==x && enemy.state.y==y);
        p.destroyed=0;opponent_step(&enemy,&p,&pilot,1);CHECK(enemy.ticks==ticks);
    }
    practice_reset(&p);opponent_reset(&enemy,&p);threat_reset(&threat);
    CHECK(p.target.x==650. && p.target.y==140. && !enemy.ticks && !enemy.state.vx && !enemy.state.vy);
    /* Lead uses target-relative velocity, not a current-position turret. */
    p.target=(BoltTarget){0.,0.,0,&p.masks[1]};p.target_vx=p.target_vy=0.;
    pilot.state=(MotionState){0.,-150.,2.,0.,0};threat.ticks=119;
    threat_step(&threat,&p,&pilot);
    CHECK(threat.shots==1 && threat.bolts[0].angle>0 && threat.bolts[0].angle<16384 && threat.bolts[0].vx>0.);
    /* Distant or unreachable targets do not consume hostile pool/cooldown. */
    threat_reset(&threat);threat.ticks=119;pilot.state=(MotionState){0.,-3000.,0.,0.,0};
    threat_step(&threat,&p,&pilot);CHECK(!threat.shots && !threat.cooldown);
    threat_reset(&threat);threat.ticks=119;pilot.state=(MotionState){0.,-150.,0.,-20.,0};
    threat_step(&threat,&p,&pilot);CHECK(!threat.shots && !threat.cooldown);
    target_power_tests(&p,&enemy,&threat,&pilot);
    puts("status=pass\nphysical_approach=pass\nbounded_turn=pass\npostmove_target_pose=pass\ndeath_and_reset=pass\nlead_and_range_gate=pass");
    return 0;
}
