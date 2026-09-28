/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "opponent.h"
#include <string.h>
static void publish(const Opponent *opponent,Practice *practice)
{
    practice->target=(BoltTarget){opponent->state.x,opponent->state.y,
        opponent->state.angle,&practice->masks[1]};
    practice->target_vx=opponent->state.vx;
    practice->target_vy=opponent->state.vy;
}
int opponent_load(Opponent *opponent,const char *path)
{
    MotionParameters parameters;
    PropulsionProfile drive;
    if(!pursuit_load(&parameters,path) || !propulsion_load(&drive,"BARGEPRO.DAT")) return 0;
    memset(opponent,0,sizeof(*opponent));
    opponent->parameters=parameters;opponent->drive=drive;
    return 1;
}
void opponent_reset(Opponent *opponent,Practice *practice)
{
    opponent->state=(MotionState){650.,140.,0.,0.,0};
    opponent->ticks=opponent->thrust_ticks=opponent->turn_ticks=0;
    practice->pursuit_target=opponent->enabled;
    practice->target_vx=practice->target_vy=0.;
    if(opponent->enabled) publish(opponent,practice);
}
void opponent_step(Opponent *opponent,Practice *practice,const Pilot *pilot,int player_destroyed)
{
    MotionCommand command;
    if(!opponent->enabled || practice->destroyed || player_destroyed) return;
    command=pursuit_command(&opponent->state,&opponent->parameters,pilot->state.x,pilot->state.y);
    target_power_tick(&practice->target_power,&practice->health);
    if(practice->target_power.enabled && practice->target_power.disabled) {
        opponent->state.vx*=1.-opponent->parameters.drag;
        opponent->state.vy*=1.-opponent->parameters.drag;
        opponent->state.x+=opponent->state.vx;opponent->state.y+=opponent->state.vy;
        command=(MotionCommand){0};
    } else if(practice->target_power.enabled) {
        propulsion_step(&opponent->state,&opponent->parameters,&command,
            &practice->target_power.resources,&practice->target_power.budget,&opponent->drive);
    } else motion_step(&opponent->state,&opponent->parameters,&command);
    ++opponent->ticks;
    opponent->thrust_ticks+=!!command.forward;
    opponent->turn_ticks+=command.turn!=0.;
    publish(opponent,practice);
}
