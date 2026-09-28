/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_TARGET_POWER_H
#define FLIGHT_TARGET_POWER_H
#include "../resources/resources.h"
#include "../shields/shields.h"
#include "../damage/damage.h"
typedef struct {
    ResourceProfile budget;
    ResourceState resources;
    ShieldProfile shield_profile;
    ShieldState shield;
    double max_hull,minimum_hull;
    unsigned blocked_energy,blocked_heat;
    int enabled,disabled,stock,stress;
} TargetPower;
int target_power_load(TargetPower *power,int stock,int stress);
void target_power_reset(TargetPower *power,DamageState *health);
void target_power_tick(TargetPower *power,DamageState *health);
void target_power_hit(TargetPower *power,DamageState *health,double shields,double hull);
int target_power_can_fire(TargetPower *power);
void target_power_fire(TargetPower *power);
#endif
