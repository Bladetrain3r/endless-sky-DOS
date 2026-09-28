/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "power.h"
#include <math.h>
/* Caller generates resources first, then moves, then attempts weapon fire. */
void pilot_powered_step(Pilot *pilot,Scene *scene,ResourceState *resources,
                        const ResourceProfile *budget,const PropulsionProfile *drive)
{
    propulsion_step(&pilot->state,&pilot->parameters,&pilot->command,resources,budget,drive);
    ++pilot->ticks;
    if(pilot->follow) {
        scene->camera_x=(int)lround(pilot->state.x)-400;
        scene->camera_y=(int)lround(pilot->state.y)-300;
    }
}

/* Native disabled drift; no control or resource expenditure. */
void pilot_disabled_step(Pilot *pilot,Scene *scene)
{
    pilot->state.vx*=1.-pilot->parameters.drag;
    pilot->state.vy*=1.-pilot->parameters.drag;
    pilot->state.x+=pilot->state.vx; pilot->state.y+=pilot->state.vy;
    ++pilot->ticks;
    if(pilot->follow) {
        scene->camera_x=(int)lround(pilot->state.x)-400;
        scene->camera_y=(int)lround(pilot->state.y)-300;
    }
}
