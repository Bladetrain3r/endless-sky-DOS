/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "projectile.h"
#include "../flight/practice.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
/* Headless mechanics checker does not draw; full app tests exercise rendering. */
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char color)
{ (void)f; (void)x; (void)y; (void)s; (void)color; }
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *blend,const unsigned char *add)
{ (void)f;(void)s;(void)h;(void)x;(void)y;(void)blend;(void)add; }
static void native_trace(const char *path)
{
    FILE *f=fopen(path,"rb"); uint32_t count,life,i,checked=0,cases=0; double speed,maxerror=0.; Bolt b={0};
    CHECK(f && fread(&count,4,1,f)==1 && count>0 && count<10000);
    CHECK(fread(&speed,8,1,f)==1 && fread(&life,4,1,f)==1);
    for(i=0;i<count;++i) {
        uint32_t u[6]; double d[8],actual[4]; unsigned k;
        CHECK(fread(u,4,6,f)==6 && fread(d,8,8,f)==8);
        if(u[1]>life) continue; /* diagnostic native Move after engine-pruned row */
        if(!u[1]) { bolt_launch(&b,d[0],d[1],d[2],d[3],(uint16_t)u[2],speed,(int)life); ++cases; }
        else bolt_move(&b);
        CHECK(b.angle==u[3] && b.alive==!u[5] && (b.lifetime<=0)==(int)u[4]);
        actual[0]=b.x;actual[1]=b.y;actual[2]=b.vx;actual[3]=b.vy;
        for(k=0;k<4;++k) {
            double error=fabs(actual[k]-d[k+4]);
            CHECK(isfinite(error) && error<=1e-10);
            if(error>maxerror) maxerror=error;
        }
        ++checked;
    }
    CHECK(fgetc(f)==EOF);fclose(f);
    printf("native_cases=%u\nnative_rows=%u\nmax_state_error=%.17g\n",(unsigned)cases,(unsigned)checked,maxerror);
}
static void sweeps(void)
{
    CollisionMask box={0}; Bolt b; double f; int hit;
    BoltTarget targets[2];
    box.radius=sqrt(2.);box.outlines=1;box.points=4;box.ends[0]=4;
    box.vertices[0]=(MaskPoint){-1,-1};box.vertices[1]=(MaskPoint){1,-1};
    box.vertices[2]=(MaskPoint){1,1};box.vertices[3]=(MaskPoint){-1,1};
    targets[0]=(BoltTarget){20,0,0,&box}; targets[1]=(BoltTarget){10,0,0,&box};
    bolt_launch(&b,0,0,0,0,16384,30,48);
    /* Fast shot tunnels past both endpoint tests: nearest segment target wins,
     * even when the far target occurs first in the lookup order. */
    hit=bolt_hit(&b,targets,2,&f); CHECK(hit==1 && fabs(f-.3)<1e-10);
    targets[0]=targets[1]; CHECK(bolt_hit(&b,targets,2,&f)==0); /* explicit stable tie */
    b.x=10;b.y=0;CHECK(bolt_hit(&b,targets,1,&f)==0 && f==0.);
    b.x=0;b.y=3;CHECK(bolt_hit(&b,targets,1,&f)==-1 && f==1.);
    b.x=0;b.y=0;b.vx=9;b.vy=0;
    CHECK(bolt_hit(&b,targets,1,&f)==-1 && f==1.); /* exact terminal boundary */
    b.lifetime=1;bolt_move(&b);
    CHECK(!b.alive && b.x==0 && bolt_hit(&b,targets,1,&f)==-1);
    printf("sweep_cases=6\n");
}
static void trainer(const char *profile,const char *masks)
{
    static Practice p; Pilot pilot={0}; unsigned i;
    CHECK(practice_load(&p,profile,masks));
    pilot.state=(MotionState){400,300,0,0,0};
    for(i=0;i<600;++i) practice_step(&p,&pilot,1);
    CHECK(p.shots==50 && p.hits>=45 && p.active<=4 && p.dropped==0);
    for(i=0;i<60;++i) practice_step(&p,&pilot,0);
    CHECK(p.shots==50 && p.hits==50 && p.active==0);
    practice_reset(&p); CHECK(p.shots==0 && p.hits==0 && p.active==0 && p.rng==17);
    pilot.state.y=-1000.;
    for(i=0;i<600;++i) practice_step(&p,&pilot,1);
    CHECK(p.shots==50 && p.hits==0 && p.peak<=4 && p.dropped==0);
    for(i=0;i<60;++i) practice_step(&p,&pilot,0);
    CHECK(p.active==0);
    /* Saturation never overwrites an active shot. Deliberately exceed the
     * training profile's required capacity to exercise the explicit drop path. */
    practice_reset(&p);p.reload=1;p.lifetime=120;
    for(i=0;i<100;++i) practice_step(&p,&pilot,1);
    CHECK(p.active==PRACTICE_BOLTS && p.shots==PRACTICE_BOLTS && p.dropped==84);
    printf("trainer_ticks=1420\ntrainer_test_hits=50\ncapacity_drop_test=84\n");
}
static unsigned moving_run(Practice *p,int lead)
{
    Pilot pilot={0}; unsigned tick;
    pilot.state=(MotionState){400,300,0,0,0};
    practice_reset(p);
    for(tick=0;tick<960;++tick) {
        double t=0.,x=0.,y=0.; unsigned j;
        for(j=0;j<12;++j) {
            double phase=(tick+1+t)*6.2831853071795864769/480.;
            x=120.*sin(phase); y=-100.-60.*cos(phase);
            t=lead?(sqrt(x*x+y*y)-20.)/p->speed:0.;
            if(t<0.) t=0.;
        }
        pilot.state.angle=motion_angle(atan2(x,-y)*180./3.14159265358979323846);
        practice_step(p,&pilot,1);
    }
    for(tick=0;tick<60;++tick) practice_step(p,&pilot,0);
    CHECK(p->shots==80 && p->dropped==0 && p->active==0);
    return p->hits;
}
static void moving(const char *profile,const char *masks)
{
    static Practice p; Pilot pilot={0}; unsigned tick,lead,direct,repeat,witnesses=0;
    CHECK(practice_load(&p,profile,masks));p.moving_target=1;practice_reset(&p);
    CHECK(p.target.x==400. && p.target.y==140. && p.target.angle==16384);
    for(tick=0;tick<480;++tick) {
        BoltTarget previous=p.target; double x,y,f0,f1;
        /* Find and fire a zero-speed diagnostic query that distinguishes the
         * previous pose from this tick's pose. This catches one-tick lag and
         * accidental extra target-velocity subtraction in integration. */
        p.target_ticks++;practice_target(&p);
        for(x=-24;x<=24 && !witnesses;x+=.5) for(y=-24;y<=24 && !witnesses;y+=.5) {
            Bolt b; bolt_launch(&b,p.target.x+x,p.target.y+y,0,0,0,0,48);
            f0=1.;f1=1.;bolt_hit(&b,&previous,1,&f0);bolt_hit(&b,&p.target,1,&f1);
            if(f0!=0. && f1==0.) {
                p.target_ticks--;p.target=previous;p.bolts[0]=b;
                practice_step(&p,&pilot,0);CHECK(p.hits==1 && !p.bolts[0].alive);++witnesses;
            }
        }
        CHECK(p.target.x>=280. && p.target.x<=520. && p.target.y>=140. && p.target.y<=260.);
    }
    CHECK(witnesses==1 && p.target_ticks==480 && fabs(p.target.x-400.)<1e-12 && fabs(p.target.y-140.)<1e-12);
    lead=moving_run(&p,1);repeat=moving_run(&p,1);direct=moving_run(&p,0);
    CHECK(lead==repeat && lead>=60 && lead>direct);
    printf("moving_route_ticks=480\npostmove_witnesses=%u\nlead_hits=%u\nunled_hits=%u\nmoving_shots=80\n",witnesses,lead,direct);
}
static void destruction(const char *profile,const char *masks,const char *damage)
{
    static Practice p; Pilot pilot={0}; unsigned i,tick;
    CHECK(practice_load(&p,profile,masks) && practice_damage_load(&p,damage));
    p.destructible=1;practice_reset(&p);pilot.state=(MotionState){400,300,0,0,0};
    for(i=0;i<180;++i) practice_step(&p,&pilot,1);
    CHECK(p.destroyed && p.hits==7 && p.health.shields==0. && p.health.hull<0.);
    CHECK(p.explosion==0);tick=p.target_ticks;
    for(i=0;i<120;++i) practice_step(&p,&pilot,1);
    CHECK(p.hits==7 && p.target_ticks==tick); /* Dead target no longer collides/moves. */
    practice_reset(&p);CHECK(!p.destroyed && p.health.shields==30. && p.health.hull==26. && p.explosion==0);
    for(i=0;i<120 && !p.destroyed;++i) practice_step(&p,&pilot,1);
    CHECK(p.destroyed && p.hits==7 && p.explosion==48);
    {
        static unsigned char frame[480032]; Scene scene={0}; Sprite barge={0};
        uint32_t rng=p.rng; unsigned n;
        /* Only the procedural effect draws in this headless checker; sprite
         * and text calls are stubs. Exercise every age at viewport edges. */
        for(n=0;n<48;++n) {
            unsigned k; memset(frame,0xcd,sizeof(frame));
            p.explosion=48-n;scene.camera_x=(int)n*20-80;scene.camera_y=(int)n*15-80;
            practice_draw(frame+16,&p,&scene,&barge,NULL,NULL);
            for(k=0;k<16;++k) CHECK(frame[k]==0xcd && frame[480016+k]==0xcd);
            CHECK(p.rng==rng);
        }
    }
    practice_reset(&p);CHECK(!p.destroyed && !p.explosion && !p.hits && !p.active);
    p.moving_target=1;practice_reset(&p);
    for(i=0;i<7;++i) {
        double phase=(p.target_ticks+1)*6.2831853071795864769/480.;
        bolt_launch(&p.bolts[0],400.+120.*sin(phase),200.-60.*cos(phase),0,0,0,0,48);
        practice_step(&p,&pilot,0);
    }
    CHECK(p.destroyed && p.hits==7);tick=p.target_ticks;
    for(i=0;i<60;++i) practice_step(&p,&pilot,0);
    CHECK(p.target_ticks==tick && !p.explosion);
    printf("destruction_hits=7\ndead_target_excluded=pass\nreset_during_explosion=pass\n");
}
static void budgets(const char *profile,const char *masks,const char *resource)
{
    static Practice p; Pilot pilot={0}; unsigned i,shots; uint32_t rng;
    ResourceProfile original;
    CHECK(practice_load(&p,profile,masks) && practice_resources_load(&p,resource,0));
    original=p.resource_profile;pilot.state=(MotionState){400,300,0,0,0};
    for(i=0;i<960;++i) practice_step(&p,&pilot,1);
    CHECK(p.shots==80 && !p.blocked_energy && !p.blocked_heat);
    CHECK(p.resources.heat>0. && p.resources.energy>original.capacity-original.shot_energy);
    CHECK(practice_resources_load(&p,resource,1));practice_reset(&p);
    for(i=0;i<960;++i) practice_step(&p,&pilot,1);
    CHECK(p.shots<80 && p.blocked_energy>0 && p.blocked_heat==0);
    shots=p.shots;
    for(i=0;i<240;++i) practice_step(&p,&pilot,0);
    CHECK(p.shots==shots && p.resources.energy>=p.resource_profile.capacity);
    practice_step(&p,&pilot,1);CHECK(p.shots==shots+1);
    /* A denied attempt neither advances spread RNG nor starts a reload. */
    p.resources.energy=0.;p.cooldown=0;rng=p.rng;shots=p.shots;
    practice_step(&p,&pilot,1);
    CHECK(p.shots==shots && p.rng==rng && !p.cooldown);
    CHECK(practice_resources_load(&p,resource,2));practice_reset(&p);
    for(i=0;i<1800;++i) practice_step(&p,&pilot,1);
    CHECK(p.shots<150 && p.blocked_heat>0 && !p.blocked_energy);
    shots=p.shots;
    for(i=0;i<600;++i) practice_step(&p,&pilot,0);
    CHECK(p.shots==shots && !p.resources.overheated);
    practice_step(&p,&pilot,1);CHECK(p.shots==shots+1);
    p.resources.overheated=1;p.resources.heat=p.resource_profile.max_heat*1.1;
    p.cooldown=0;rng=p.rng;shots=p.shots;
    practice_step(&p,&pilot,1);
    CHECK(p.shots==shots && p.rng==rng && !p.cooldown);
    practice_reset(&p);
    CHECK(p.resources.energy==p.resource_profile.capacity && p.resources.heat==0. &&
          !p.resources.overheated && !p.blocked_energy && !p.blocked_heat && p.rng==17);
    CHECK(p.resource_profile.max_heat==450.); /* Reset retains the selected preset. */
    {
        ResourceState expected=p.resources;
        resource_tick(&expected,&p.resource_profile);
        for(i=0;i<PRACTICE_BOLTS;++i) bolt_launch(&p.bolts[i],-1000,-1000,0,0,0,0,100);
        practice_step(&p,&pilot,1);
        CHECK(p.dropped==1 && p.shots==0 && p.resources.energy==expected.energy &&
              p.resources.heat==expected.heat && p.rng==17);
    }
    printf("resource_composition=pass\nenergy_starve_recover=pass\nheat_lock_recover=pass\n");
}
int main(int argc,char **argv)
{
    if(argc!=6) return 2;
    native_trace(argv[1]);sweeps();trainer(argv[2],argv[3]);moving(argv[2],argv[3]);destruction(argv[2],argv[3],argv[4]);
    budgets(argv[2],argv[3],argv[5]);
    printf("status=pass\n");return 0;
}
