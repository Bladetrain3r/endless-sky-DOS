/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "scene.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#ifdef REFERENCE_RENDERER
#define paint sprite_draw_reference
#else
#define paint sprite_draw
#endif
static double clock_ms(void)
{
#ifdef __DJGPP__
    return (double)uclock()*1000./UCLOCKS_PER_SEC;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec*1000.+t.tv_nsec/1000000.;
#endif
}
/* Prototype date=0 ephemeris; Angle convention follows System::SetDate.
   No time advancement/calendar, gameplay AI, collision or native ship physics. */
int scene_init(Scene *scene,const ActiveSystem *system,unsigned ships)
{
    double x[256],y[256]; int earth=-1,luna=-1; unsigned i;
    if(system->object_count>256 || !ships || ships>64) return 0;
    memset(scene,0,sizeof(*scene)); scene->count=ships;
    for(i=0;i<system->object_count;++i) {
        const WorldObject *o=&system->objects[i];
        long step=lround(fmod(o->offset,360.0)*65536.0/360.0);
        double angle=(step&65535)*6.2831853071795864769/65536.;
        x[i]=sin(angle)*o->distance; y[i]=-cos(angle)*o->distance;
        if(o->parent>=0) { x[i]+=x[o->parent]; y[i]+=y[o->parent]; }
        if(o->sprite && !strcmp(o->sprite,"planet/earth")) earth=(int)i;
        if(o->sprite && !strcmp(o->sprite,"planet/luna")) luna=(int)i;
    }
    if(earth<0 || luna<0) return 0;
    scene->earth_x=550; scene->earth_y=345;
    scene->luna_x=550+(int)lround(x[luna]-x[earth]);
    scene->luna_y=345+(int)lround(y[luna]-y[earth]);
    for(i=0;i<ships;++i) {
        scene->ships[i].x=(int)(70+i*113)%800;
        scene->ships[i].y=90+(int)(i*83)%410;
        scene->ships[i].kind=i%2; scene->ships[i].heading=(i*3)%16;
    }
    return 1;
}
void scene_step(Scene *scene)
{
    unsigned i; ++scene->ticks;
    if(scene->camera_enabled) {
        int phase=(int)(scene->ticks%960);
        int vertical=(int)(scene->ticks%720);
        scene->camera_x=phase<240?phase:(phase<720?480-phase:phase-960);
        scene->camera_y=(vertical<180?vertical:(vertical<540?360-vertical:vertical-720))/2;
    }
    for(i=0;i<scene->count;++i) {
        Traffic *t=&scene->ships[i];
        t->x+=1+(int)(i%3);
        if(t->x>870) t->x=-70;
        if(scene->ticks%60==0) t->heading=(t->heading+1)%16;
    }
}
void scene_draw(unsigned char *frame,const Scene *scene,const Sprite sprites[6],
                const unsigned char *blend,const unsigned char *add,SceneProfile *profile)
{
    uint32_t rng=17; unsigned i;
    double start=profile?clock_ms():0.,now;
    int cx=scene->camera_x,cy=scene->camera_y;
    memset(frame,0,480000);
    for(i=0;i<300;++i) {
        unsigned x,y; rng=rng*1664525u+1013904223u; x=rng%800;
        rng=rng*1664525u+1013904223u; y=rng%600;
        /* Distant stars move at one eighth of foreground camera speed. */
        x=(unsigned)((int)x-cx/8+800)%800;
        y=(unsigned)((int)y-cy/8+600)%600;
        frame[y*800+x]=(unsigned char)(28+i%18);
    }
    if(profile) { now=clock_ms(); profile->background_ms=now-start; start=now; }
    paint(frame,&sprites[3],0,scene->earth_x-cx,scene->earth_y-cy,blend,add);
    paint(frame,&sprites[4],0,scene->luna_x-cx,scene->luna_y-cy,blend,add);
    if(profile) { now=clock_ms(); profile->orbital_ms=now-start; start=now; }
    paint(frame,&sprites[2],0,170-cx,335-cy,blend,add);
    for(i=0;i<scene->count;++i) {
        const Traffic *t=&scene->ships[i];
        paint(frame,&sprites[t->kind],t->heading,t->x-cx,t->y-cy,blend,add);
    }
    if(profile) { now=clock_ms(); profile->traffic_ms=now-start; start=now; }
    /* Deliberately overlap an additive flare with opaque hull and empty sky. */
    paint(frame,&sprites[5],0,165+(int)(scene->ticks%40)-cx,330-cy,blend,add);
    if(profile) profile->effect_ms=clock_ms()-start;
}
