/* Stock Sparrow shield recharge subset of Endless Sky.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "shields.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int shield_load(ShieldProfile *profile, const char *path)
{
    FILE *file;
    unsigned char bytes[72];
    double values[7];
    ShieldProfile parsed;
    unsigned i, j;
    uint32_t delay[2] = {0, 0};
    if(!profile || !path || sizeof(double) != 8 || !(file = fopen(path, "rb"))) return 0;
    if(fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes) || fgetc(file) != EOF || ferror(file)) {
        fclose(file); return 0;
    }
    fclose(file);
    if(memcmp(bytes, "ESSHLD2\0", 8)) return 0;
    for(i = 0; i < 7; ++i) {
        uint64_t bits = 0;
        for(j = 0; j < 8; ++j) bits |= (uint64_t)bytes[8 + i * 8 + j] << (8 * j);
        memcpy(&values[i], &bits, 8);
    }
    for(i = 0; i < 2; ++i)
        for(j = 0; j < 4; ++j) delay[i] |= (uint32_t)bytes[64 + i * 4 + j] << (8 * j);
    parsed = (ShieldProfile){values[0], values[1], values[2], values[3],
        values[4], values[5], values[6], (int)delay[0], (int)delay[1]};
    if(!isfinite(parsed.capacity) || parsed.capacity <= 0. || parsed.capacity > 1e9 ||
       !isfinite(parsed.rate) || parsed.rate < 0. || parsed.rate > 1e9 ||
       !isfinite(parsed.energy_cost) || parsed.energy_cost < 0. || parsed.energy_cost > 1e9 ||
       !isfinite(parsed.heat_cost) || parsed.heat_cost < 0. || parsed.heat_cost > 1e9 ||
       !isfinite(parsed.delayed_rate) || parsed.delayed_rate < 0. || parsed.delayed_rate > 1e9 ||
       !isfinite(parsed.delayed_energy_cost) || parsed.delayed_energy_cost < 0. || parsed.delayed_energy_cost > 1e9 ||
       !isfinite(parsed.delayed_heat_cost) || parsed.delayed_heat_cost < 0. || parsed.delayed_heat_cost > 1e9 ||
       delay[0] > 1000000 || delay[1] > 1000000) return 0;
    *profile = parsed;
    return 1;
}

void shield_reset(ShieldState *state, const ShieldProfile *profile)
{
    state->shields = profile->capacity;
    state->delay = 0;
}

void shield_tick(ShieldState *state, const ShieldProfile *profile,
    ResourceState *resources, int was_disabled)
{
    if(!was_disabled) {
        double available = state->delay ? profile->delayed_rate : profile->rate;
        double cost = state->delay ? profile->delayed_energy_cost : profile->energy_cost;
        double heat_cost = state->delay ? profile->delayed_heat_cost : profile->heat_cost;
        if(available && state->shields < profile->capacity) {
            double transfer;
            cost /= available;
            /* ResourceLevels::DoRepair limits energy before maximum-stat transfer. */
            if(cost > 0. && resources->energy / cost < available)
                available = resources->energy / cost;
            transfer = profile->capacity - state->shields;
            if(available < transfer) transfer = available;
            if(transfer > 0.) {
                state->shields += transfer;
                resources->energy -= transfer * cost;
                /* Ship::DoGeneration divides energy by rate, but not heat. */
                resources->heat += transfer * heat_cost;
            }
        }
        if(state->delay) --state->delay;
    }
    if(state->shields > profile->capacity) state->shields = profile->capacity;
    if(state->shields < 0.) state->shields = 0.;
}

void shield_damage(ShieldState *state, const ShieldProfile *profile,
    double damage, int was_disabled)
{
    int delay;
    if(!(damage > 0.) || !isfinite(damage)) return;
    state->shields -= damage;
    if(state->shields < 0.) state->shields = 0.;
    if(was_disabled) return;
    delay = (state->shields <= 0. && profile->depleted_delay)
        ? profile->depleted_delay : profile->delay;
    if(state->delay < delay) state->delay = delay;
}
