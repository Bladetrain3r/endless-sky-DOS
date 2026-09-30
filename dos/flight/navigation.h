/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_NAVIGATION_H
#define FLIGHT_NAVIGATION_H
#include "power.h"
#include "input_state.h"
#include "threat.h"
#define NAV_DESTINATIONS 2
#define NAV_ALLOWED 1u
#define NAV_SERVICES 2u
#define NAV_SPACEPORT 4u
#define NAV_OUTFITTER 8u
#define NAV_SHIPYARD 16u
#define NAV_REPAIR_HULL 32u
#define NAV_RECHARGE_SHIELD 64u
#define NAV_RECHARGE_ENERGY 128u
#define NAV_MANUAL (INPUT_FORWARD|INPUT_BACK|INPUT_LEFT|INPUT_RIGHT)
typedef enum { NAV_FLYING, NAV_LANDING, NAV_DOCKED, NAV_LAUNCHING } NavPhase;
typedef struct {
    char name[32],description[1024];
    double x,y,radius;
    unsigned flags;
} NavDestination;
typedef struct {
    NavDestination destinations[NAV_DESTINATIONS];
    double idle_heat;
    float zoom,landing_speed;
    NavPhase phase;
    int selected,approach,release_controls;
    unsigned ticks,landings,launches,cancellations,selection_changes,notice_ticks;
    uint32_t rng;
    char notice[80];
} Navigation;
/* Initial Earth/Luna permissions/services snapshot; not live politics. */
int navigation_load(Navigation *nav,const Scene *scene,const char *path);
void navigation_reset(Navigation *nav);
void navigation_keys(Navigation *nav,Pilot *pilot,Scene *scene,unsigned keys);
/* Owns player generation/movement and quiet projectiles in navigation mode.
 * Docked ticks freeze space and resources; no elapsed-time catch-up on departure. */
void navigation_step(Navigation *nav,Pilot *pilot,Scene *scene,Practice *practice,
                     Threat *player,const PropulsionProfile *drive);
int navigation_can_land(const Navigation *nav,const Pilot *pilot,const Practice *practice,const Threat *player);
void navigation_report(const Navigation *nav);
#endif
