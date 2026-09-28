/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_PRACTICE_H
#define FLIGHT_PRACTICE_H
#include "pilot.h"
#include "../damage/damage.h"
#include "../resources/resources.h"
#include "../shields/shields.h"
#include "../projectile/projectile.h"
#define PRACTICE_BOLTS 16
/* One supplied training gun, not stock Sparrow's beam-laser armament. */
typedef struct {
    CollisionMask masks[3];
    Bolt bolts[PRACTICE_BOLTS];
    unsigned shots,hits,dropped,active,peak,flash,cooldown;
    ResourceProfile resource_profile;
    ResourceState resources;
    int resource_enabled;
    ShieldProfile shield_profile;
    ShieldState shield;
    int shield_enabled;
    unsigned shield_test_pulses;
    unsigned blocked_energy,blocked_heat;
    uint32_t rng;
    unsigned target_ticks;
    int moving_target;
    int pursuit_target;
    double target_vx,target_vy;
    BoltTarget target;
    DamageState health;
    double shield_damage,hull_damage;
    int destructible,destroyed;
    unsigned explosion;
    double speed,inaccuracy;
    int lifetime,reload;
} Practice;
int practice_load(Practice *p,const char *profile,const char *masks);
int practice_damage_load(Practice *p,const char *path);
int practice_resources_load(Practice *p,const char *path,int stress);
int practice_shields_load(Practice *p,const char *path);
void practice_shield_test(Practice *p);
void practice_reset(Practice *p);
void practice_target(Practice *p);
/* Standalone gun-only tick, retained for focused projectile tests. */
void practice_step(Practice *p,const Pilot *pilot,int fire);
void practice_begin_tick(Practice *p);
void practice_begin_tick_disabled(Practice *p,int hull_disabled);
void practice_finish_tick(Practice *p,const Pilot *pilot,int fire);
void practice_draw(unsigned char *frame,const Practice *p,const Scene *scene,
                   const Sprite *barge,const unsigned char *blend,const unsigned char *add);
#endif
