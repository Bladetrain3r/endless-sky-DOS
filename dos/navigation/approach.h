/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOS_NAVIGATION_APPROACH_H
#define DOS_NAVIGATION_APPROACH_H
#include "../motion/motion.h"

/* Stationary stellar target; healthy, fully crewed Sparrow without reverse or
 * afterburner. The caller owns landing permission, state, and motion step. */
MotionCommand navigation_approach(const MotionState *ship, const MotionParameters *params,
                                  double target_x, double target_y, double radius);
#endif
