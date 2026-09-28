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
    if(!pursuit_load(&parameters,path)) return 0;
    memset(opponent,0,sizeof(*opponent));
    opponent->parameters=parameters;
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
    /* Fully supplied Star Barge movement; enemy resources are still a trainer
     * abstraction, distinct from the player's shared resource simulation. */
    motion_step(&opponent->state,&opponent->parameters,&command);
    ++opponent->ticks;
    opponent->thrust_ticks+=!!command.forward;
    opponent->turn_ticks+=command.turn!=0.;
    publish(opponent,practice);
}
