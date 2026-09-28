/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOS_PURSUIT_H
#define DOS_PURSUIT_H
#include "../motion/motion.h"

/* ESPURS1 NUL plus five little-endian binary64 MotionParameters fields. */
int pursuit_load(MotionParameters *params, const char *path);

/* Stock Star Barge MoveToAttack: current target position, no forward guns,
 * reverse engine, or afterburner. Target selection and firing are caller-owned. */
MotionCommand pursuit_command(const MotionState *ship, const MotionParameters *params,
                              double target_x, double target_y);
double pursuit_turn_toward(const MotionState *ship, const MotionParameters *params,
                           double dx, double dy);
/* Native AI::RendezvousTime semantics, including NaN for impossible cases. */
double pursuit_intercept(double dx, double dy, double relative_vx,
                         double relative_vy, double speed);
#endif
