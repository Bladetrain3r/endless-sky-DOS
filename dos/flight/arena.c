/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arena.h"
#include "input.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int arena_init(Arena *a,const Practice *p,const Opponent *profile,int stock,int stress)
{
    TargetPower power;
    unsigned i;
    if(!target_power_load(&power,stock,stress)) return 0;
    memset(a,0,sizeof(*a));
    a->parameters=profile->parameters;a->drive=profile->drive;
    a->stock=stock;a->stress=stress;
    for(i=0;i<ARENA_ENEMIES;++i) a->enemies[i].power=power;
    arena_reset(a,p);return 1;
}
void arena_reset(Arena *a,const Practice *p)
{
    static const double spawn[ARENA_ENEMIES][2]={{190.,80.},{680.,120.}};
    unsigned i;
    a->ticks=a->selection_changes=0;a->selected=0;
    for(i=0;i<ARENA_ENEMIES;++i) {
        ArenaEnemy *e=&a->enemies[i];
        TargetPower power=e->power;
        memset(e,0,sizeof(*e));e->power=power;
        target_power_reset(&e->power,&e->health);
        e->motion=(MotionState){spawn[i][0],spawn[i][1],0.,0.,32768};
        e->target=(BoltTarget){e->motion.x,e->motion.y,e->motion.angle,&p->masks[1]};
    }
}
unsigned arena_alive(const Arena *a)
{
    unsigned i,n=0;
    for(i=0;i<ARENA_ENEMIES;++i) n+=!a->enemies[i].destroyed;
    return n;
}
static int nearest(const Arena *a,const Pilot *pilot)
{
    unsigned i;int selected=-1;double best=0.;
    for(i=0;i<ARENA_ENEMIES;++i) if(!a->enemies[i].destroyed) {
        double dx=a->enemies[i].motion.x-pilot->state.x,dy=a->enemies[i].motion.y-pilot->state.y;
        double distance=dx*dx+dy*dy;
        if(selected<0 || distance<best) { selected=(int)i;best=distance; }
    }
    return selected;
}
void arena_keys(Arena *a,const Pilot *pilot,unsigned pressed)
{
    int selected=a->selected;unsigned n;
    if(pressed & INPUT_TARGET_NEXT) {
        for(n=1;n<=ARENA_ENEMIES;++n) {
            int next=(a->selected+(int)n)%(int)ARENA_ENEMIES;
            if(!a->enemies[next].destroyed) { selected=next;break; }
        }
    }
    if(pressed & INPUT_TARGET_NEAREST) selected=nearest(a,pilot);
    if(selected!=a->selected) { a->selected=selected;++a->selection_changes; }
}
static void move_enemy(Arena *a,ArenaEnemy *e,const Practice *p,const Pilot *pilot,int player_dead)
{
    MotionCommand command;
    if(e->flash) --e->flash;
    if(e->explosion) --e->explosion;
    if(e->destroyed || player_dead) return;
    target_power_tick(&e->power,&e->health);
    command=pursuit_command(&e->motion,&a->parameters,pilot->state.x,pilot->state.y);
    if(e->power.disabled) {
        e->motion.vx*=1.-a->parameters.drag;e->motion.vy*=1.-a->parameters.drag;
        e->motion.x+=e->motion.vx;e->motion.y+=e->motion.vy;
    } else propulsion_step(&e->motion,&a->parameters,&command,&e->power.resources,
                            &e->power.budget,&a->drive);
    e->target=(BoltTarget){e->motion.x,e->motion.y,e->motion.angle,&p->masks[1]};
}
static void friendly_hits(Arena *a,Practice *p)
{
    unsigned i,j;
    p->active=0;
    for(i=0;i<PRACTICE_BOLTS;++i) {
        Bolt *bolt=&p->bolts[i];
        BoltTarget targets[ARENA_ENEMIES];unsigned slots[ARENA_ENEMIES],count=0;
        double fraction;int hit;
        /* Rebuild after each hit so a destroyed hull cannot catch later shots.
         * Compact index maps to stable ship slots, never to selection order. */
        for(j=0;j<ARENA_ENEMIES;++j) if(!a->enemies[j].destroyed) {
            slots[count]=j;targets[count++]=a->enemies[j].target;
        }
        hit=bolt_hit(bolt,targets,count,&fraction);
        if(hit>=0) {
            ArenaEnemy *e=&a->enemies[slots[hit]];
            bolt->alive=0;++p->hits;p->flash=e->flash=8;
            if(p->destructible) {
                target_power_hit(&e->power,&e->health,p->shield_damage,p->hull_damage);
                if(e->health.hull<0.) { e->destroyed=1;e->explosion=48; }
            }
        }
        p->active+=!!bolt->alive;
    }
    if(p->active>p->peak) p->peak=p->active;
}
static void hostile_step(Arena *a,ArenaEnemy *e,Practice *p,const Pilot *pilot,Threat *player)
{
    unsigned i;
    double dx=pilot->state.x-e->motion.x,dy=pilot->state.y-e->motion.y;
    double vx=pilot->state.vx-e->motion.vx,vy=pilot->state.vy-e->motion.vy;
    double time=pursuit_intercept(dx,dy,vx,vy,p->speed);
    BoltTarget target={pilot->state.x,pilot->state.y,pilot->state.angle,&p->masks[0]};
    if(e->cooldown) --e->cooldown;
    for(i=0;i<ARENA_BOLTS;++i) bolt_move(&e->bolts[i]);
    if(!e->destroyed && !player->destroyed && a->ticks>=THREAT_GRACE_TICKS && !e->cooldown &&
       isfinite(time) && time>=0. && time<=p->lifetime && target_power_can_fire(&e->power)) {
        for(i=0;i<ARENA_BOLTS && e->bolts[i].alive;++i) {}
        if(i<ARENA_BOLTS) {
            double ux,uy;
            uint16_t angle=motion_angle(atan2(dx+time*vx,-dy-time*vy)*180./3.14159265358979323846);
            motion_unit(angle,&ux,&uy);
            bolt_launch(&e->bolts[i],e->motion.x+20.*ux-.5*e->motion.vx,
                        e->motion.y+20.*uy-.5*e->motion.vy,
                        e->motion.vx,e->motion.vy,angle,p->speed,p->lifetime);
            target_power_fire(&e->power);++e->shots;
        } else ++e->dropped;
        e->cooldown=(unsigned)p->reload;
    }
    e->active=0;
    for(i=0;i<ARENA_BOLTS;++i) {
        Bolt *bolt=&e->bolts[i];double fraction;
        if(!player->destroyed && bolt_hit(bolt,&target,1,&fraction)>=0) {
            bolt->alive=0;++e->hits;threat_hit(player,p);
        }
        e->active+=!!bolt->alive;
    }
}
void arena_step(Arena *a,Practice *p,Pilot *pilot,Threat *player,int fire)
{
    unsigned i;
    ++a->ticks;++player->ticks;
    if(player->flash) --player->flash;
    for(i=0;i<ARENA_ENEMIES;++i) move_enemy(a,&a->enemies[i],p,pilot,player->destroyed);
    practice_fire_tick(p,pilot,fire && !player->disabled && !player->destroyed);
    friendly_hits(a,p);
    player->shots=player->active=player->dropped=0;
    for(i=0;i<ARENA_ENEMIES;++i) {
        ArenaEnemy *e=&a->enemies[i];
        hostile_step(a,e,p,pilot,player);
        player->shots+=e->shots;player->active+=e->active;player->dropped+=e->dropped;
    }
    if(player->active>player->peak) player->peak=player->active;
    if(a->selected<0 || a->enemies[a->selected].destroyed) a->selected=nearest(a,pilot);
}
void arena_report(const Arena *a)
{
    unsigned i;
    printf("arena=1\narena_alive=%u\narena_selected=%d\narena_selection_changes=%u\narena_ticks=%u\n",
           arena_alive(a),a->selected,a->selection_changes,a->ticks);
    for(i=0;i<ARENA_ENEMIES;++i) {
        const ArenaEnemy *e=&a->enemies[i];
        printf("arena_%u_x=%.17g\narena_%u_y=%.17g\narena_%u_shields=%.17g\narena_%u_hull=%.17g\n",
            i,e->motion.x,i,e->motion.y,i,e->health.shields,i,e->health.hull);
        printf("arena_%u_energy=%.17g\narena_%u_heat=%.17g\narena_%u_disabled=%d\narena_%u_destroyed=%d\n",
            i,e->power.resources.energy,i,e->power.resources.heat,i,e->power.disabled,i,e->destroyed);
        printf("arena_%u_shots=%u\narena_%u_hits=%u\narena_%u_active=%u\narena_%u_dropped=%u\n",
            i,e->shots,i,e->hits,i,e->active,i,e->dropped);
    }
}
