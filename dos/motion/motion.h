/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOS_MOTION_H
#define DOS_MOTION_H

#include <stdint.h>

/* Units match upstream: world units, world units/tick, degrees/tick; 60 Hz.
 * This slice requires a healthy, adequately crewed and supplied ship with
 * ordinary forward thrust. No afterburner, status effects or external forces.
 * Resource accounting and capability resolution belong to the caller. */
typedef struct {
    double x, y, vx, vy;
    uint16_t angle;
} MotionState;

typedef struct {
    double acceleration, reverse_acceleration, turn_rate;
    double drag, acceleration_multiplier;
} MotionParameters;

typedef struct {
    int forward, back, stop;
    double turn;
} MotionCommand;

uint16_t motion_angle(double degrees);
void motion_unit(uint16_t angle, double *x, double *y);
void motion_step(MotionState *state, const MotionParameters *parameters,
                 const MotionCommand *command);

#endif
