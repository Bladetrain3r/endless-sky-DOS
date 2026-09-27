/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_PRACTICE_H
#define FLIGHT_PRACTICE_H
#include "pilot.h"
#include "../projectile/projectile.h"
#define PRACTICE_BOLTS 16
/* One supplied training gun, not stock Sparrow's beam-laser armament. */
typedef struct {
    CollisionMask masks[3];
    Bolt bolts[PRACTICE_BOLTS];
    unsigned shots,hits,dropped,active,peak,flash,cooldown;
    uint32_t rng;
    unsigned target_ticks;
    int moving_target;
    BoltTarget target;
    double speed,inaccuracy;
    int lifetime,reload;
} Practice;
int practice_load(Practice *p,const char *profile,const char *masks);
void practice_reset(Practice *p);
void practice_target(Practice *p);
void practice_step(Practice *p,const Pilot *pilot,int fire);
void practice_draw(unsigned char *frame,const Practice *p,const Scene *scene,
                   const Sprite *barge,const unsigned char *blend,const unsigned char *add);
#endif
