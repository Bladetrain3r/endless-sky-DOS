/* Bounded stock Sparrow propulsion resource coupling.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ES_DOS_PROPULSION_H
#define ES_DOS_PROPULSION_H
#include "../motion/motion.h"
#include "../resources/resources.h"

typedef struct PropulsionProfile {
    double thrust_energy, thrust_heat, turn_energy, turn_heat;
} PropulsionProfile;

/* ESPROP2 binary64 profile. Failure leaves the destination untouched. */
int propulsion_load(PropulsionProfile *profile, const char *path);
/* Caller runs resource_tick first, then this step, then optional resource_fire.
 * This function includes position integration. Stock Sparrow has no reverse. */
void propulsion_step(MotionState *motion, const MotionParameters *parameters,
                     const MotionCommand *command, ResourceState *resources,
                     const ResourceProfile *resource_profile,
                     const PropulsionProfile *propulsion_profile);
#endif
