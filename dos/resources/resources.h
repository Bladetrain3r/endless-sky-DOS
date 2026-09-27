/* Passive firing energy and heat subset of Endless Sky's Ship resources.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ES_DOS_RESOURCES_H
#define ES_DOS_RESOURCES_H

typedef struct ResourceProfile {
    double capacity, generation, heat_generation, dissipation, max_heat;
    double shot_energy, shot_heat;
} ResourceProfile;

typedef struct ResourceState {
    double energy, heat;
    int overheated;
} ResourceState;

/* Validated ESRESOURCE1 text profile; returns 1 on success, 0 on failure. */
int resource_load(ResourceProfile *profile, const char *path);
void resource_reset(ResourceState *state, const ResourceProfile *profile);
void resource_tick(ResourceState *state, const ResourceProfile *profile);
int resource_can_fire(const ResourceState *state, const ResourceProfile *profile);
int resource_fire(ResourceState *state, const ResourceProfile *profile);
#endif
