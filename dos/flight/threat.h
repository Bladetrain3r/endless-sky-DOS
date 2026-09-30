/* Scripted incoming-fire trainer. Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FLIGHT_THREAT_H
#define FLIGHT_THREAT_H
#include "practice.h"
#define THREAT_BOLTS 16
#define THREAT_GRACE_TICKS 120
typedef struct {
    Bolt bolts[THREAT_BOLTS];
    unsigned ticks,shots,hits,dropped,active,peak,flash,cooldown;
    double max_hull,minimum_hull,hull;
    double previous_target_x,previous_target_y;
    int have_previous_target,enabled,disabled,destroyed;
} Threat;
/* ESPLAYER1 + two little-endian binary64: native MaxHull, MinimumHull. */
int threat_load(Threat *threat,const char *path);
void threat_reset(Threat *threat);
/* Call after practice_finish_tick, using the player's post-move pose. */
/* One ordinary projectile impact on the single player hull/shield state. */
void threat_hit(Threat *threat,Practice *practice);
void threat_step(Threat *threat,Practice *practice,const Pilot *pilot);
void threat_draw(unsigned char *frame,const Threat *threat,const Scene *scene);
#endif
