/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "practice.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* An eight-second ellipse, not ship AI or a flight-dynamics model. */
void practice_target(Practice *p)
{
    double phase=(p->target_ticks%480)*6.2831853071795864769/480.;
    p->target=(BoltTarget){400.,140.,0,&p->masks[1]};
    if(p->moving_target) {
        p->target.x=400.+120.*sin(phase);
        p->target.y=200.-60.*cos(phase);
        p->target.angle=motion_angle(atan2(120.*cos(phase),-60.*sin(phase))*180./3.14159265358979323846);
    }
}
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
int practice_damage_load(Practice *p,const char *path)
{
    FILE *f=fopen(path,"r"); char magic[16],extra; double shield,hull; int n;
    if(!f) return 0;
    n=fscanf(f,"%15s %lf %lf %c",magic,&shield,&hull,&extra); fclose(f);
    if(n!=3 || strcmp(magic,"ESDAMAGE1") || !isfinite(shield) || !isfinite(hull) ||
       shield<=0. || shield>1000. || hull<=0. || hull>1000.) return 0;
    p->shield_damage=shield;p->hull_damage=hull;return 1;
}
/* Stress profiles are deliberate trainer overrides, not native ship stats. */
int practice_resources_load(Practice *p,const char *path,int stress)
{
    if(!resource_load(&p->resource_profile,path)) return 0;
    if(stress==1) { p->resource_profile.capacity=40.; p->resource_profile.generation=.25; }
    else if(stress==2) { p->resource_profile.max_heat=450.; p->resource_profile.heat_generation=0.; }
    p->resource_enabled=1;
    resource_reset(&p->resources,&p->resource_profile);
    return 1;
}
int practice_shields_load(Practice *p,const char *path)
{
    if(!shield_load(&p->shield_profile,path)) return 0;
    p->shield_enabled=1;
    shield_reset(&p->shield,&p->shield_profile);
    return 1;
}
/* Explicit trainer input, not a weapon or a native combat damage event. */
void practice_shield_test(Practice *p)
{
    if(!p->shield_enabled) return;
    shield_damage(&p->shield,&p->shield_profile,p->shield_profile.capacity*.25,
                  p->resources.overheated);
    ++p->shield_test_pulses;
}
void practice_reset(Practice *p)
{
    memset(p->bolts,0,sizeof(p->bolts));
    p->shots=p->hits=p->dropped=p->active=p->peak=p->flash=p->cooldown=0;
    p->blocked_energy=p->blocked_heat=0;
    p->shield_test_pulses=0;
    if(p->shield_enabled) shield_reset(&p->shield,&p->shield_profile);
    if(p->resource_enabled) resource_reset(&p->resources,&p->resource_profile);
    p->rng=17; p->target_ticks=0;
    p->health=(DamageState){30.,26.}; /* Reduced training target, not stock Barge stats. */
    target_power_reset(&p->target_power,&p->health);
    p->destroyed=0; p->explosion=0;
    practice_target(p);
}
static double random_unit(Practice *p)
{
    p->rng=p->rng*1664525u+1013904223u;
    return (double)p->rng/4294967296.;
}
void practice_begin_tick(Practice *p)
{
    practice_begin_tick_disabled(p,0);
}
void practice_begin_tick_disabled(Practice *p,int hull_disabled)
{
    /* Native repair spends last tick's remainder before generation. Full-health
     * Sparrow has no other disabled cause here; NeedsEnergy only gates movement. */
    if(p->shield_enabled && p->resource_enabled)
        shield_tick(&p->shield,&p->shield_profile,&p->resources,p->resources.overheated || hull_disabled);
    if(p->resource_enabled) resource_tick_disabled(&p->resources,&p->resource_profile,hull_disabled);
}
void practice_step(Practice *p,const Pilot *pilot,int fire)
{
    practice_begin_tick(p);
    practice_finish_tick(p,pilot,fire);
}
void practice_fire_tick(Practice *p,const Pilot *pilot,int fire)
{
    unsigned i;
    if(p->flash) --p->flash;
    if(p->cooldown) --p->cooldown;
    /* Existing shots move first. New shots join for this tick's collision pass
     * without receiving a Move, like Engine::CalculateUnpaused. */
    for(i=0;i<PRACTICE_BOLTS;++i) bolt_move(&p->bolts[i]);
    if(fire && !p->cooldown && p->resource_enabled &&
       !resource_can_fire(&p->resources,&p->resource_profile)) {
        if(p->resources.overheated) ++p->blocked_heat;
        else ++p->blocked_energy;
        fire=0; /* No RNG consumption, reload or cost for a denied attempt. */
    }
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
            if(p->resource_enabled) resource_fire(&p->resources,&p->resource_profile);
        } else ++p->dropped;
        p->cooldown=(unsigned)p->reload;
    }
}
void practice_finish_tick(Practice *p,const Pilot *pilot,int fire)
{
    unsigned i;
    if(p->explosion) --p->explosion;
    if(!p->destroyed) {
        ++p->target_ticks;
        if(!p->pursuit_target) practice_target(p); /* Post-move collision/render pose. */
    }
    practice_fire_tick(p,pilot,fire);
    p->active=0;
    for(i=0;i<PRACTICE_BOLTS;++i) {
        Bolt *b=&p->bolts[i]; double fraction;
        if(!p->destroyed && bolt_hit(b,&p->target,1,&fraction)>=0) {
            b->alive=0; ++p->hits; p->flash=8;
            if(p->destructible) {
                target_power_hit(&p->target_power,&p->health,p->shield_damage,p->hull_damage);
                if(p->health.hull<0.) { p->destroyed=1; p->explosion=48; }
            }
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
    int tx=(int)lround(p->target.x)-scene->camera_x,ty=(int)lround(p->target.y)-scene->camera_y;
    unsigned i;
    if(!p->destroyed) sprite_draw(frame,barge,((unsigned)p->target.angle*barge->frames+32768u)/65536u,tx,ty,blend,add);
    /* Bounds also keep the fixed-width text renderer safely inside its frame. */
    if(tx>=55 && tx<745 && ty>=72 && ty<535)
        video_text(frame,tx-48,ty-32,p->destroyed?"DESTROYED - R":(p->target_power.disabled?"DISABLED":(p->flash?"HIT!":(p->pursuit_target?"PURSUIT TARGET":(p->moving_target?"MOVING TARGET":"PRACTICE TARGET")))),p->flash?1:2);
    if(p->explosion && tx>-80 && tx<880 && ty>-80 && ty<680) {
        /* Bounded procedural sparks; no native effect timing/art claim. Render
         * never consumes the firing RNG, so frame rate cannot alter accuracy. */
        unsigned age=48-p->explosion;
        for(i=0;i<24;++i) {
            double angle=i*6.2831853071795864769/24.;
            double radius=3.+age*(.6+(i%5)*.11),ux=sin(angle),uy=cos(angle);
            int k; unsigned char color=age<8?1:(age<24?6:4);
            for(k=0;k<3;++k) dot(frame,tx+(int)lround((radius-k)*ux),ty+(int)lround((radius-k)*uy),color);
        }
    }
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
