/* Passive firing energy and heat subset of Endless Sky's Ship resources.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "resources.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int resource_load(ResourceProfile *profile, const char *path)
{
    FILE *file;
    ResourceProfile parsed;
    char header[32], tail[2];
    int fields;
    if(!profile || !path || !(file = fopen(path, "r"))) return 0;
    fields = fscanf(file, "%31s", header);
    if(fields != 1 || strcmp(header, "ESRESOURCE1")) { fclose(file); return 0; }
    fields = fscanf(file, "%lf %lf %lf %lf %lf %lf %lf", &parsed.capacity,
        &parsed.generation, &parsed.heat_generation, &parsed.dissipation,
        &parsed.max_heat, &parsed.shot_energy, &parsed.shot_heat);
    if(fields != 7 || fscanf(file, "%1s", tail) != EOF || ferror(file)) {
        fclose(file); return 0;
    }
    fclose(file);
    if(!isfinite(parsed.capacity) || parsed.capacity < 0. || parsed.capacity > 1e9 ||
        !isfinite(parsed.generation) || fabs(parsed.generation) > 1e9 ||
        !isfinite(parsed.heat_generation) || fabs(parsed.heat_generation) > 1e9 ||
        !isfinite(parsed.dissipation) || parsed.dissipation < 0. || parsed.dissipation > 1. ||
        !isfinite(parsed.max_heat) || parsed.max_heat <= 0. || parsed.max_heat > 1e9 ||
        !isfinite(parsed.shot_energy) || parsed.shot_energy < 0. || parsed.shot_energy > 1e9 ||
        !isfinite(parsed.shot_heat) || parsed.shot_heat < 0. || parsed.shot_heat > 1e9) return 0;
    *profile = parsed;
    return 1;
}

void resource_reset(ResourceState *state, const ResourceProfile *profile)
{
    state->energy = profile->capacity;
    state->heat = 0.;
    state->overheated = 0;
}

void resource_tick(ResourceState *state, const ResourceProfile *profile)
{
    /* Ship::DoGeneration clamps before production, allowing temporary overflow. */
    if(state->energy > profile->capacity) state->energy = profile->capacity;
    state->heat -= state->heat * profile->dissipation;
    if(state->heat > profile->max_heat) state->overheated = 1;
    else if(state->heat < .9 * profile->max_heat) state->overheated = 0;
    if(!state->overheated) {
        state->energy += profile->generation;
        state->heat += profile->heat_generation;
    }
    if(state->energy < 0.) state->energy = 0.;
    if(state->heat < 0.) state->heat = 0.;
}

int resource_can_fire(const ResourceState *state, const ResourceProfile *profile)
{
    return !state->overheated && state->energy >= profile->shot_energy;
}

int resource_fire(ResourceState *state, const ResourceProfile *profile)
{
    if(!resource_can_fire(state, profile)) return 0;
    state->energy -= profile->shot_energy;
    state->heat += profile->shot_heat;
    if(state->heat < 0.) state->heat = 0.;
    return 1;
}
