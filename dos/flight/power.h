/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_POWER_H
#define FLIGHT_POWER_H
#include "pilot.h"
#include "../propulsion/propulsion.h"
void pilot_powered_step(Pilot *pilot,Scene *scene,ResourceState *resources,
                        const ResourceProfile *budget,const PropulsionProfile *drive);
void pilot_disabled_step(Pilot *pilot,Scene *scene);
#endif
