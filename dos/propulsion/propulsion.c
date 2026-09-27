/* Ship::DoMovement resource order adapted from Endless Sky Ship.cpp.
 * Original work Copyright (c) 2014 Michael Zahniser and contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "propulsion.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int propulsion_load(PropulsionProfile *profile, const char *path)
{
    FILE *file;
    PropulsionProfile parsed;
    char magic[8], tail;
    if(!profile || !path || !(file = fopen(path, "rb"))) return 0;
    if(fread(magic, 1, 8, file) != 8 || memcmp(magic, "ESPROP2\0", 8)
        || fread(&parsed, sizeof(double), 4, file) != 4
        || fread(&tail, 1, 1, file) != 0 || ferror(file)) {
        fclose(file); return 0;
    }
    fclose(file);
    if(!isfinite(parsed.thrust_energy) || parsed.thrust_energy < 0. || parsed.thrust_energy > 1e9 ||
       !isfinite(parsed.thrust_heat) || parsed.thrust_heat < 0. || parsed.thrust_heat > 1e9 ||
       !isfinite(parsed.turn_energy) || parsed.turn_energy < 0. || parsed.turn_energy > 1e9 ||
       !isfinite(parsed.turn_heat) || parsed.turn_heat < 0. || parsed.turn_heat > 1e9) return 0;
    *profile = parsed;
    return 1;
}

static double available_fraction(double energy, double cost)
{
    /* ResourceLevels::FractionalUsage initializes scale to 1. */
    return cost > 0. && energy < cost ? energy / cost : 1.;
}

void propulsion_step(MotionState *motion, const MotionParameters *parameters,
                     const MotionCommand *command, ResourceState *resources,
                     const ResourceProfile *resource_profile,
                     const PropulsionProfile *propulsion_profile)
{
    MotionParameters adjusted = *parameters;
    MotionCommand applied = *command;
    double turn, throttle;
    int needs_energy = resource_profile->capacity > 0.
        && resources->energy < resource_profile->capacity
        && resources->energy <= .00390625
        && resource_profile->generation <= 0.
        && (propulsion_profile->thrust_energy > 0. || propulsion_profile->turn_energy > 0.);
    if(resources->overheated || needs_energy) {
        motion->vx *= 1. - parameters->drag;
        motion->vy *= 1. - parameters->drag;
        motion->x += motion->vx;
        motion->y += motion->vy;
        return;
    }
    turn = fabs(command->turn);
    if(turn) {
        double fraction = available_fraction(resources->energy, propulsion_profile->turn_energy);
        if(turn > fraction) turn = fraction;
        applied.turn = copysign(turn, command->turn);
        if(turn) {
            resources->energy -= turn * propulsion_profile->turn_energy;
            if(resources->energy < 0.) resources->energy = 0.;
            resources->heat += turn * propulsion_profile->turn_heat;
            if(resources->heat < 0.) resources->heat = 0.;
        }
    }
    throttle = !!command->forward - !!command->back;
    applied.forward = 0;
    if(throttle > 0.) {
        throttle *= available_fraction(resources->energy, propulsion_profile->thrust_energy);
        if(throttle) {
            resources->energy -= throttle * propulsion_profile->thrust_energy;
            if(resources->energy < 0.) resources->energy = 0.;
            resources->heat += throttle * propulsion_profile->thrust_heat;
            if(resources->heat < 0.) resources->heat = 0.;
            adjusted.acceleration *= throttle;
        }
        applied.forward = throttle > 0.;
    }
    /* No reverse engine: BACK and cancelled FORWARD/BACK neither spend nor brake. */
    applied.back = 0;
    motion_step(motion, &adjusted, &applied);
}
