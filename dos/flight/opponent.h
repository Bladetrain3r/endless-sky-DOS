/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_OPPONENT_H
#define FLIGHT_OPPONENT_H
#include "practice.h"
#include "../pursuit/pursuit.h"
typedef struct {
    MotionState state;
    MotionParameters parameters;
    unsigned ticks,thrust_ticks,turn_ticks;
    int enabled;
} Opponent;
int opponent_load(Opponent *opponent,const char *path);
void opponent_reset(Opponent *opponent,Practice *practice);
void opponent_step(Opponent *opponent,Practice *practice,const Pilot *pilot,int player_destroyed);
#endif
