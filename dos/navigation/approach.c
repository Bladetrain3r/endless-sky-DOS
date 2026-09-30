/* SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from Endless Sky AI.cpp, Copyright (c) 2014 Michael Zahniser.
 */
#include "approach.h"
#include "../pursuit/pursuit.h"
#include <math.h>

static double length(double x, double y) { return sqrt(x * x + y * y); }
static double unit_x(double x, double y) { double n=length(x,y); return n ? x * (1. / n) : 1.; }
static double unit_y(double x, double y) { double n=length(x,y); return n ? y * (1. / n) : 0.; }
static double clamp(double x) { return x < -1. ? -1. : (x > 1. ? 1. : x); }

MotionCommand navigation_approach(const MotionState *s, const MotionParameters *p,
                                  double tx, double ty, double radius)
{
    MotionCommand c={0,0,0,0.};
    double dx=tx-s->x, dy=ty-s->y, v=length(s->vx,s->vy);
    double fx,fy, degrees, stop_distance, sx,sy, tvx,tvy, max_velocity, facing;
    int is_close=length(dx,dy)<radius;
    if(is_close && v<1.) return c;
    /* AI::StoppingPoint with no reverse engine and targetVelocity=(0,0). */
    sx=s->x; sy=s->y;
    motion_unit(s->angle,&fx,&fy);
    if(v) {
        degrees=(180./3.14159265358979323846)*acos(clamp(-unit_x(s->vx,s->vy)*fx-unit_y(s->vx,s->vy)*fy));
        stop_distance=v*(degrees/p->turn_rate)+.5*v*v/p->acceleration;
        sx+=stop_distance*unit_x(s->vx,s->vy);
        sy+=stop_distance*unit_y(s->vx,s->vy);
    }
    dx=tx-sx; dy=ty-sy;
    tvx=unit_x(dx,dy); tvy=unit_y(dx,dy);
    facing=tvx*fx+tvy*fy;
    if(!is_close || facing<=.95)
        c.turn=pursuit_turn_toward(s,p,dx,dy);
    max_velocity=(p->acceleration/p->drag)*.99;
    if(facing>.95 && (v*v<=max_velocity*max_velocity
                       || unit_x(dx,dy)*unit_x(s->vx,s->vy)+unit_y(dx,dy)*unit_y(s->vx,s->vy)<.95))
        c.forward=1;
    return c;
}
