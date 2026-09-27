/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "practice.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* The trainer's only hittable object. Background traffic is cosmetic. */
#define TARGET_X 400.
#define TARGET_Y 140.
int practice_load(Practice *p,const char *profile,const char *masks)
{
    FILE *f=fopen(profile,"r"); char magic[16],extra; double speed,spread; int life,reload,n;
    if(!f) return 0;
    n=fscanf(f,"%15s %lf %d %d %lf %c",magic,&speed,&life,&reload,&spread,&extra);
    fclose(f);
    if(n!=5 || strcmp(magic,"ESBOLT1") || !isfinite(speed) || speed<=0. || speed>100. ||
       life<1 || life>120 || reload<1 || reload>120 || !isfinite(spread) || spread<0. || spread>10.) return 0;
    memset(p,0,sizeof(*p));
    if(!masks_load(masks,p->masks,3)) return 0;
    p->speed=speed; p->lifetime=life; p->reload=reload; p->inaccuracy=spread;
    practice_reset(p);
    return 1;
}
void practice_reset(Practice *p)
{
    memset(p->bolts,0,sizeof(p->bolts));
    p->shots=p->hits=p->dropped=p->active=p->peak=p->flash=p->cooldown=0;
    p->rng=17;
}
static double random_unit(Practice *p)
{
    p->rng=p->rng*1664525u+1013904223u;
    return (double)p->rng/4294967296.;
}
void practice_step(Practice *p,const Pilot *pilot,int fire)
{
    unsigned i;
    BoltTarget target={TARGET_X,TARGET_Y,0,&p->masks[1]};
    if(p->flash) --p->flash;
    if(p->cooldown) --p->cooldown;
    /* Existing shots move first. New shots join for this tick's collision pass
     * without receiving a Move, like Engine::CalculateUnpaused. */
    for(i=0;i<PRACTICE_BOLTS;++i) bolt_move(&p->bolts[i]);
    if(fire && !p->cooldown) {
        for(i=0;i<PRACTICE_BOLTS && p->bolts[i].alive;++i) {}
        if(i<PRACTICE_BOLTS) {
            double ux,uy,first=random_unit(p),second=random_unit(p);
            uint16_t angle=pilot->state.angle+motion_angle((first-second)*p->inaccuracy);
            motion_unit(pilot->state.angle,&ux,&uy);
            /* Artificial centreline muzzle 20 world units forward, with native
             * Hardpoint::Fire's half-parent-velocity launch correction. */
            bolt_launch(&p->bolts[i],pilot->state.x+20.*ux-.5*pilot->state.vx,
                        pilot->state.y+20.*uy-.5*pilot->state.vy,
                        pilot->state.vx,pilot->state.vy,angle,p->speed,p->lifetime);
            ++p->shots;
        } else ++p->dropped;
        p->cooldown=(unsigned)p->reload;
    }
    p->active=0;
    for(i=0;i<PRACTICE_BOLTS;++i) {
        Bolt *b=&p->bolts[i]; double fraction;
        if(bolt_hit(b,&target,1,&fraction)>=0) {
            b->alive=0; ++p->hits; p->flash=8;
        }
        if(b->alive) ++p->active;
    }
    if(p->active>p->peak) p->peak=p->active;
}
static void dot(unsigned char *frame,int x,int y,unsigned char color)
{
    if(x>=0 && x<800 && y>=40 && y<570) frame[y*800+x]=color;
}
void practice_draw(unsigned char *frame,const Practice *p,const Scene *scene,
                   const Sprite *barge,const unsigned char *blend,const unsigned char *add)
{
    int tx=(int)TARGET_X-scene->camera_x,ty=(int)TARGET_Y-scene->camera_y;
    unsigned i;
    sprite_draw(frame,barge,0,tx,ty,blend,add);
    /* Bounds also keep the fixed-width text renderer safely inside its frame. */
    if(tx>=55 && tx<745 && ty>=72 && ty<535)
        video_text(frame,tx-48,ty-32,p->flash?"HIT!":"PRACTICE TARGET",p->flash?1:2);
    for(i=0;i<PRACTICE_BOLTS;++i) if(p->bolts[i].alive) {
        const Bolt *b=&p->bolts[i]; double ux,uy; int k;
        double x=b->x+.5*b->vx-scene->camera_x,y=b->y+.5*b->vy-scene->camera_y;
        motion_unit(b->angle,&ux,&uy);
        /* Simple seven-pixel tracer; native projectile art/stretch is not ported.
         * Cull doubles before rounding so distant shots cannot overflow ints. */
        if(x< -8. || x>808. || y<32. || y>578.) continue;
        for(k=-3;k<=3;++k) dot(frame,(int)lround(x+k*ux),(int)lround(y+k*uy),1);
    }
}
