/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_ARENA_H
#define FLIGHT_ARENA_H
#include "opponent.h"
#include "threat.h"
#define ARENA_ENEMIES 2
#define ARENA_BOLTS 16
typedef struct {
    MotionState motion;
    TargetPower power;
    DamageState health;
    BoltTarget target;
    Bolt bolts[ARENA_BOLTS];
    unsigned shots,hits,dropped,active,cooldown,flash,explosion;
    int destroyed;
} ArenaEnemy;
typedef struct {
    ArenaEnemy enemies[ARENA_ENEMIES];
    MotionParameters parameters;
    PropulsionProfile drive;
    unsigned ticks,selection_changes;
    int selected,stock,stress;
} Arena;
/* Optional arena mode: two hostile Barge trainers; no friendly fire, collisions
 * between ships, boundary force, loot or native fleet AI. Static arena layout. */
int arena_init(Arena *arena,const Practice *practice,const Opponent *profile,int stock,int stress);
void arena_reset(Arena *arena,const Practice *practice);
void arena_keys(Arena *arena,const Pilot *pilot,unsigned pressed);
unsigned arena_alive(const Arena *arena);
/* Player generation/motion precedes this. All enemies move before collision;
 * friendly hits precede enemy firing. Threat holds the single player's hull. */
void arena_step(Arena *arena,Practice *practice,Pilot *pilot,Threat *player,int fire);
void arena_report(const Arena *arena);
#endif
