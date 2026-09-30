/* SPDX-License-Identifier: GPL-3.0-or-later
 * Landing interpolation adapted from Endless Sky Ship.cpp (Michael Zahniser).
 * Bounded quiet Earth/Luna navigation; native service/permission snapshot.
 */
#include "navigation.h"
#include "input.h"
#include "../navigation/approach.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static double number(const unsigned char *p)
{
    uint64_t bits=0;double value;unsigned i;
    for(i=0;i<8;++i) bits|=(uint64_t)p[i]<<(i*8);
    memcpy(&value,&bits,8);return value;
}
static int text_valid(const char *text,size_t size,int multiline)
{
    size_t i;
    for(i=0;i<size;++i) {
        unsigned char c=(unsigned char)text[i];
        if(!c) return i>0;
        if(c<32 || c>126) { if(!multiline || c!='\n') return 0; }
    }
    return 0;
}
int navigation_load(Navigation *nav,const Scene *scene,const char *path)
{
    Navigation parsed={0};unsigned char header[24],record[1068];unsigned i,j;
    FILE *f=fopen(path,"rb");int ok=0;
    if(!f) return 0;
    if(sizeof(double)!=8 || fread(header,1,sizeof(header),f)!=sizeof(header) ||
       memcmp(header,"ESNAV1\0\0",8)) goto done;
    parsed.idle_heat=number(header+8);parsed.landing_speed=(float)number(header+16);
    if(!isfinite(parsed.idle_heat) || parsed.idle_heat<0. || parsed.idle_heat>1e9 ||
       !isfinite(parsed.landing_speed) || parsed.landing_speed<=0. || parsed.landing_speed>1.) goto done;
    for(i=0;i<NAV_DESTINATIONS;++i) {
        NavDestination *d=&parsed.destinations[i];
        if(fread(record,1,sizeof(record),f)!=sizeof(record)) goto done;
        memcpy(d->name,record,32);memcpy(d->description,record+32,1024);
        if(!text_valid(d->name,32,0) || !text_valid(d->description,1024,1)) goto done;
        if(strcmp(d->name,i?"Luna":"Earth")) goto done;
        for(j=0;j<4;++j) d->flags|=(unsigned)record[1056+j]<<(8*j);
        d->radius=number(record+1060);
        if(d->flags>255 || !isfinite(d->radius) || d->radius<=0. || d->radius>2048.) goto done;
        d->x=i?scene->luna_x:scene->earth_x;d->y=i?scene->luna_y:scene->earth_y;
    }
    if(fgetc(f)!=EOF || ferror(f)) goto done;
    navigation_reset(&parsed);*nav=parsed;ok=1;
done:
    fclose(f);return ok;
}
void navigation_reset(Navigation *n)
{
    n->zoom=1.f;n->phase=NAV_FLYING;n->selected=0;n->approach=n->release_controls=0;
    n->ticks=n->landings=n->launches=n->cancellations=n->selection_changes=n->notice_ticks=0;
    n->rng=71;n->notice[0]=0;
}
static void notice(Navigation *n,const char *text)
{
    snprintf(n->notice,sizeof(n->notice),"%s",text);n->notice_ticks=180;
}
static double random_unit(Navigation *n)
{
    n->rng=n->rng*1664525u+1013904223u;
    return n->rng/4294967296.;
}
static void camera(const Pilot *p,Scene *scene)
{
    if(p->follow) {
        scene->camera_x=(int)lround(p->state.x)-400;
        scene->camera_y=(int)lround(p->state.y)-300;
    }
}
void navigation_keys(Navigation *n,Pilot *p,Scene *scene,unsigned keys)
{
    unsigned pressed=keys & ~p->previous_keys;
    if(n->phase==NAV_DOCKED) {
        if(pressed & INPUT_LAND) {
            const NavDestination *d=&n->destinations[n->selected];double ux,uy,r;
            /* Same angle/radial distribution as Engine::Place; trainer LCG,
             * deliberately separate from the firing RNG and native RNG stream. */
            p->state.angle=(uint16_t)(random_unit(n)*65536.);
            motion_unit(p->state.angle,&ux,&uy);r=random_unit(n)*d->radius;
            p->state.x=d->x+ux*r;p->state.y=d->y+uy*r;
            p->state.vx=ux;p->state.vy=uy;p->command=(MotionCommand){0};
            n->zoom=0.f;n->phase=NAV_LAUNCHING;n->release_controls=1;++n->launches;
            notice(n,"DEPARTING");camera(p,scene);
        }
        return;
    }
    if(n->phase!=NAV_FLYING) return;
    if(pressed & INPUT_PLANET) {
        n->selected=(n->selected+1)%NAV_DESTINATIONS;++n->selection_changes;
        if(n->approach) ++n->cancellations;
        n->approach=0;notice(n,"DESTINATION SELECTED");
    }
    if(keys & NAV_MANUAL) {
        if(n->approach) {n->approach=0;++n->cancellations;notice(n,"APPROACH CANCELLED");}
    } else if(pressed & INPUT_LAND) {
        if(n->approach) {n->approach=0;++n->cancellations;notice(n,"APPROACH CANCELLED");}
        else if(!(n->destinations[n->selected].flags & NAV_ALLOWED)) notice(n,"LANDING PERMISSION DENIED");
        else {n->approach=1;notice(n,"AUTOPILOT APPROACH");}
    }
}
int navigation_can_land(const Navigation *n,const Pilot *p,const Practice *practice,const Threat *player)
{
    const NavDestination *d=&n->destinations[n->selected];
    double dx=d->x-p->state.x,dy=d->y-p->state.y;
    return (d->flags & NAV_ALLOWED) && !player->disabled && !player->destroyed &&
        !practice->resources.overheated && sqrt(p->state.vx*p->state.vx+p->state.vy*p->state.vy)<1. &&
        sqrt(dx*dx+dy*dy)<d->radius;
}
static void dock(Navigation *n,Practice *p,Threat *player)
{
    unsigned flags=n->destinations[n->selected].flags;
    n->zoom=0.f;n->phase=NAV_DOCKED;n->approach=0;++n->landings;
    /* Supported Earth/Luna ports restore these existing ship systems. There
     * is no fuel/cargo/crew/economy state yet. No timer advances while docked. */
    if(flags & NAV_SERVICES) {
        if(flags & NAV_RECHARGE_SHIELD) shield_reset(&p->shield,&p->shield_profile);
        if(flags & NAV_RECHARGE_ENERGY) p->resources.energy=p->resource_profile.capacity;
        if(flags & NAV_REPAIR_HULL) player->hull=player->max_hull;
        p->resources.heat=n->idle_heat;p->resources.overheated=0;
    }
    player->disabled=player->hull<player->minimum_hull;
    memset(p->bolts,0,sizeof(p->bolts));p->active=p->cooldown=0;
    notice(n,"DOCKED");
}
void navigation_step(Navigation *n,Pilot *p,Scene *scene,Practice *practice,
                     Threat *player,const PropulsionProfile *drive)
{
    const NavDestination *d=&n->destinations[n->selected];unsigned i;
    if(n->phase==NAV_DOCKED) return;
    ++n->ticks;
    if(n->notice_ticks) --n->notice_ticks;
    practice_begin_tick_disabled(practice,player->disabled);
    if(n->phase==NAV_LANDING && (player->disabled || player->destroyed || practice->resources.overheated)) {
        n->phase=NAV_LAUNCHING;n->approach=0;n->release_controls=1;
        ++n->cancellations;notice(n,"LANDING INTERRUPTED");
    }
    if(n->phase==NAV_LANDING || n->phase==NAV_LAUNCHING) {
        ++p->ticks;
        p->command=(MotionCommand){0};
        if(n->phase==NAV_LANDING) {
            /* Native binary32 zoom and default .02f transition; no control/fire
             * during the descent. Dock panel replaces full PlayerInfo::Land. */
            p->state.x=.97*p->state.x+.03*d->x;p->state.y=.97*p->state.y+.03*d->y;
            n->zoom-=n->landing_speed;
            if(n->zoom<=0.f) {dock(n,practice,player);camera(p,scene);return;}
        } else {
            n->zoom+=n->landing_speed;
            if(n->zoom>=1.f) {n->zoom=1.f;n->phase=NAV_FLYING;notice(n,"FLIGHT CONTROLS READY");}
        }
        p->state.x+=p->state.vx*n->zoom;p->state.y+=p->state.vy*n->zoom;
    } else {
        unsigned controls=p->previous_keys & (NAV_MANUAL|INPUT_FIRE);
        if(!controls) n->release_controls=0;
        if(n->release_controls) p->command=(MotionCommand){0};
        if(n->approach) {
            if(player->disabled || player->destroyed) {n->approach=0;notice(n,"SHIP UNABLE TO LAND");}
            else if(navigation_can_land(n,p,practice,player)) {
                n->phase=NAV_LANDING;p->command=(MotionCommand){0};
                notice(n,"LANDING"); /* Native initialization tick still coasts once. */
            } else p->command=navigation_approach(&p->state,&p->parameters,d->x,d->y,d->radius);
        }
        if(player->destroyed) ++p->ticks;
        else if(player->disabled) pilot_disabled_step(p,scene);
        else pilot_powered_step(p,scene,&practice->resources,&practice->resource_profile,drive);
    }
    practice_fire_tick(practice,p,n->phase==NAV_FLYING && !n->release_controls &&
                       !player->disabled && !player->destroyed && !!(p->previous_keys & INPUT_FIRE));
    practice->active=0;
    for(i=0;i<PRACTICE_BOLTS;++i) practice->active+=!!practice->bolts[i].alive;
    if(practice->active>practice->peak) practice->peak=practice->active;
    camera(p,scene);
}
void navigation_report(const Navigation *n)
{
    printf("navigation=1\nnav_phase=%d\nnav_selected=%d\nnav_approach=%d\nnav_ticks=%u\n"
        "nav_landings=%u\nnav_launches=%u\nnav_cancellations=%u\nnav_selection_changes=%u\nnav_zoom=%.9g\n",
        n->phase,n->selected,n->approach,n->ticks,n->landings,n->launches,n->cancellations,n->selection_changes,n->zoom);
}
