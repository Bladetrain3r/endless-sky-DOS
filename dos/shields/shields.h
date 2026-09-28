/* Stock Sparrow shield recharge subset of Endless Sky.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ES_DOS_SHIELDS_H
#define ES_DOS_SHIELDS_H
#include "../resources/resources.h"

typedef struct ShieldProfile {
    double capacity, rate, energy_cost, heat_cost;
    double delayed_rate, delayed_energy_cost, delayed_heat_cost;
    int delay, depleted_delay;
} ShieldProfile;

typedef struct ShieldState {
    double shields;
    int delay;
} ShieldState;

/* ESSHLD2\0, seven little-endian binary64 values, two little-endian uint32 values. */
int shield_load(ShieldProfile *profile, const char *path);
void shield_reset(ShieldState *state, const ShieldProfile *profile);
/* Call before resource_tick; was_disabled is Ship::isDisabled on entry to DoGeneration. */
void shield_tick(ShieldState *state, const ShieldProfile *profile,
    ResourceState *resources, int was_disabled);
/* Bounded damage helper: positive shield damage only, no hull/status effects. */
void shield_damage(ShieldState *state, const ShieldProfile *profile,
    double damage, int was_disabled);
#endif
