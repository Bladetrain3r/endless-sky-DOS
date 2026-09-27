/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "projectile.h"
#include "../flight/practice.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
int main(int argc,char **argv)
{
    if(argc!=4) return 2;
    native_trace(argv[1]);sweeps();trainer(argv[2],argv[3]);
    printf("status=pass\n");return 0;
}
