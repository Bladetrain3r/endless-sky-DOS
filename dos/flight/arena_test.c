/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arena.h"
#include "input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
void video_text(unsigned char *f,int x,int y,const char *s,unsigned char color)
{ (void)f;(void)x;(void)y;(void)s;(void)color; }
void sprite_draw(unsigned char *f,const Sprite *s,unsigned h,int x,int y,const unsigned char *b,const unsigned char *a)
{ (void)f;(void)s;(void)h;(void)x;(void)y;(void)b;(void)a; }
static Practice p;
static Arena arena;
static Pilot pilot;
static Threat player;
static Opponent profile;
static void reset(void)
{
    practice_reset(&p);threat_reset(&player);
    CHECK(arena_init(&arena,&p,&profile,0,0));
    pilot.state=(MotionState){400.,300.,0.,0.,0};
}
static void pose(unsigned slot,double x,double y)
{
    arena.enemies[slot].motion=(MotionState){x,y,0.,0.,0};
}
static void shot(Bolt *b,double x,double y,double vx,double vy)
{
    *b=(Bolt){x,y,vx,vy,48,1,0};
}
int main(void)
{
    unsigned i;
    CHECK(practice_load(&p,"BLASTER.DAT","MASKS.BIN"));
    CHECK(practice_damage_load(&p,"DAMAGE.DAT") && practice_shields_load(&p,"SHIELD.DAT"));
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",0));
    CHECK(opponent_load(&profile,"PURSUIT.DAT") && threat_load(&player,"PLAYER.DAT"));
    p.destructible=1;reset();
    /* Both ships pursue and budget independently, using the real profiles. */
    for(i=0;i<300;++i) {
        practice_begin_tick_disabled(&p,player.disabled);
        arena_step(&arena,&p,&pilot,&player,0);
    }
    CHECK(arena.enemies[0].shots && arena.enemies[1].shots);
    CHECK(arena.enemies[0].motion.x!=190. && arena.enemies[1].motion.x!=680.);
    CHECK(player.hits==arena.enemies[0].hits+arena.enemies[1].hits && player.hits>0);
    CHECK(player.shots==arena.enemies[0].shots+arena.enemies[1].shots);
    CHECK(arena.enemies[0].power.resources.energy!=arena.enemies[1].power.resources.energy);
    reset();CHECK(arena.ticks==0 && arena_alive(&arena)==2 && arena.selected==0);
    CHECK(!arena.enemies[0].shots && !arena.enemies[1].shots && !p.shots && player.hull==300.);
    /* Health/resource pools have no aliasing; a depleted/overheated owner
     * cannot suppress the other ship's due shot. */
    arena.ticks=119;
    arena.enemies[0].power.resources.energy=0.;
    arena.enemies[0].power.budget.generation=0.;
    arena_step(&arena,&p,&pilot,&player,0);
    CHECK(!arena.enemies[0].shots && arena.enemies[1].shots==1);
    reset();arena.ticks=119;
    arena.enemies[0].power.resources.heat=2.*arena.enemies[0].power.budget.max_heat;
    arena.enemies[0].power.resources.overheated=1;
    arena_step(&arena,&p,&pilot,&player,0);
    CHECK(!arena.enemies[0].shots && arena.enemies[1].shots==1);
    /* Selection is informational. The earliest intersected hull takes the hit,
     * even when it is slot 1 and slot 0 is selected. Freeze only locomotion. */
    reset();arena.parameters.acceleration=arena.parameters.turn_rate=0.;
    pose(0,400.,100.);pose(1,400.,200.);
    shot(&p.bolts[0],400.,600.,0.,-300.);
    arena_step(&arena,&p,&pilot,&player,0);
    CHECK(p.hits==1 && !p.bolts[0].alive);
    CHECK(arena.enemies[0].health.shields==30. && arena.enemies[1].health.shields<30.);
    /* A lethal friendly hit suppresses that owner's same-tick shot, leaves
     * the other owner operational, and automatically selects the survivor. */
    reset();arena.parameters.acceleration=arena.parameters.turn_rate=0.;arena.ticks=119;
    pose(0,400.,100.);arena.enemies[0].health=(DamageState){0.,0.};
    shot(&p.bolts[0],400.,100.,0.,0.);
    arena_step(&arena,&p,&pilot,&player,0);
    CHECK(arena.enemies[0].destroyed && !arena.enemies[0].shots);
    CHECK(arena.enemies[1].shots==1 && arena.selected==1 && arena_alive(&arena)==1);
    /* A merely disabled enemy likewise cannot fire in the same tick. */
    reset();arena.parameters.acceleration=arena.parameters.turn_rate=0.;arena.ticks=119;
    pose(0,400.,100.);arena.enemies[0].health=(DamageState){0.,10.};
    shot(&p.bolts[0],400.,100.,0.,0.);
    arena_step(&arena,&p,&pilot,&player,0);
    CHECK(arena.enemies[0].power.disabled && !arena.enemies[0].destroyed && !arena.enemies[0].shots);
    /* Flying shots outlive their source, even after both sources die.
     * Both impacts debit the SAME player shield/hull rather than private copies. */
    reset();p.shield.shields=0.;
    for(i=0;i<ARENA_ENEMIES;++i) {
        arena.enemies[i].destroyed=1;
        shot(&arena.enemies[i].bolts[0],400.,300.,0.,0.);
    }
    arena_step(&arena,&p,&pilot,&player,0);
    CHECK(player.hits==2 && fabs(player.hull-(300.-2.*p.hull_damage))<1e-9);
    CHECK(arena.selected==-1 && !arena_alive(&arena) && !player.active);
    arena_keys(&arena,&pilot,INPUT_TARGET_NEXT|INPUT_TARGET_NEAREST);CHECK(arena.selected==-1);
    /* Explicit cycle/nearest and destroyed-skip behavior. */
    reset();arena_keys(&arena,&pilot,INPUT_TARGET_NEXT);CHECK(arena.selected==1);
    arena_keys(&arena,&pilot,INPUT_TARGET_NEXT);CHECK(arena.selected==0);
    pose(0,10000.,10000.);pose(1,400.,310.);
    arena_keys(&arena,&pilot,INPUT_TARGET_NEAREST);CHECK(arena.selected==1);
    arena.enemies[0].destroyed=1;
    arena_keys(&arena,&pilot,INPUT_TARGET_NEXT);CHECK(arena.selected==1);
    /* Filling one owner's pool cannot consume energy for a dropped shot,
     * overflow memory, or block the other owner. Compare an otherwise identical
     * full-pool tick with a cooldown-gated one to isolate engine costs. */
    {
        Arena other;Practice q;Threat t;
        reset();arena.ticks=119;
        for(i=0;i<ARENA_BOLTS;++i) shot(&arena.enemies[0].bolts[i],10000.,10000.,0.,0.);
        other=arena;q=p;t=player;other.enemies[0].cooldown=10;
        arena_step(&arena,&p,&pilot,&player,0);arena_step(&other,&q,&pilot,&t,0);
        CHECK(arena.enemies[0].dropped==1 && !arena.enemies[0].shots && arena.enemies[1].shots==1);
        CHECK(arena.enemies[0].active==ARENA_BOLTS);
        CHECK(arena.enemies[0].power.resources.energy==other.enemies[0].power.resources.energy);
        CHECK(arena.enemies[0].power.resources.heat==other.enemies[0].power.resources.heat);
    }
    reset();
    CHECK(arena.enemies[0].power.resources.energy==4000. && arena.enemies[0].health.shields==30.);
    CHECK(!arena.enemies[0].bolts[0].alive && !arena.enemies[1].bolts[0].alive && !arena.selection_changes);
    arena_step(&arena,&p,&pilot,&player,1);CHECK(p.shots==1 && !player.shots);
    puts("status=pass\nindependent_lifecycles=pass\nnearest_collision_not_selection=pass\nsame_tick_kill_and_disable=pass\nsource_death_bolts_and_shared_player=pass\nselection_and_reset=pass\nfixed_owner_pools=pass");
    return 0;
}
