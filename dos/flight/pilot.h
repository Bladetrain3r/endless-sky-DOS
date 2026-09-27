/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_PILOT_H
#define FLIGHT_PILOT_H
#include "scene.h"
#include "../motion/motion.h"
typedef struct {
    MotionState state;
    MotionParameters parameters;
    MotionCommand command;
    unsigned previous_keys, ticks;
    int follow;
} Pilot;
int pilot_load(Pilot *pilot, const char *path);
void pilot_reset(Pilot *pilot, Scene *scene);
void pilot_keys(Pilot *pilot, Scene *scene, unsigned keys);
void pilot_step(Pilot *pilot, Scene *scene);
void pilot_draw(unsigned char *frame, const Pilot *pilot, const Scene *scene,
                const Sprite *sprite, const unsigned char *blend, const unsigned char *add);
unsigned pilot_replay_keys(unsigned tick);
#endif
