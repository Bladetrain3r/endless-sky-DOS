/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#ifdef REFERENCE_RENDERER
#define paint sprite_draw_reference
#else
#define paint sprite_draw
#endif
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
    for(i=0;i<scene->count;++i) {
        Traffic *t=&scene->ships[i];
        t->x+=1+(int)(i%3);
        if(t->x>870) t->x=-70;
        if(scene->ticks%60==0) t->heading=(t->heading+1)%16;
    }
}
void scene_draw(unsigned char *frame,const Scene *scene,const Sprite sprites[6],
                const unsigned char *blend,const unsigned char *add)
{
    uint32_t rng=17; unsigned i;
    memset(frame,0,480000);
    for(i=0;i<300;++i) {
        unsigned x,y; rng=rng*1664525u+1013904223u; x=rng%800;
        rng=rng*1664525u+1013904223u; y=rng%600;
        frame[y*800+x]=(unsigned char)(28+i%18);
    }
    paint(frame,&sprites[3],0,scene->earth_x,scene->earth_y,blend,add);
    paint(frame,&sprites[4],0,scene->luna_x,scene->luna_y,blend,add);
    paint(frame,&sprites[2],0,170,335,blend,add);
    for(i=0;i<scene->count;++i) {
        const Traffic *t=&scene->ships[i];
        paint(frame,&sprites[t->kind],t->heading,t->x,t->y,blend,add);
    }
    /* Deliberately overlap an additive flare with opaque hull and empty sky. */
    paint(frame,&sprites[5],0,165+(int)(scene->ticks%40),330,blend,add);
}
