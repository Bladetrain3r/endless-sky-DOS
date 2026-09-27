/* SPDX-License-Identifier: GPL-3.0-or-later
 * Movement arithmetic adapted from Endless Sky Ship.cpp and Angle.cpp.
 * Original work Copyright (c) 2014 Michael Zahniser and contributors.
 * See the repository license for GPL-3.0-or-later terms.
 */
#include "motion.h"
#include <math.h>

uint16_t motion_angle(double degrees)
{
    return (uint16_t)(llround(degrees * (65536. / 360.)) & 65535);
}

void motion_unit(uint16_t angle, double *x, double *y)
{
    /* Compute on demand instead of retaining upstream's 1 MiB lookup table.
     * Preserve all 65,536 simulation headings, independently of sprite art. */
    const double radians = angle * (3.14159265358979323846 / 32768.);
    *x = sin(radians);
    *y = -cos(radians);
}

void motion_step(MotionState *s, const MotionParameters *p, const MotionCommand *c)
{
    double ux, uy, ax, ay, dx, dy, alength, dlength, weight;
    int throttle = !!c->forward - !!c->back;
    s->angle = (uint16_t)(s->angle + motion_angle(c->turn * p->turn_rate));
    motion_unit(s->angle, &ux, &uy);
    ax = ux * throttle * (throttle > 0 ? p->acceleration : p->reverse_acceleration);
    ay = uy * throttle * (throttle > 0 ? p->acceleration : p->reverse_acceleration);

    /* A healthy ship coasts without passive drag. In particular, pressing BACK
     * without reverse engines must not slow it down. */
    if(ax != 0. || ay != 0.) {
        dx = ax - s->vx * p->drag * p->acceleration_multiplier;
        dy = ay - s->vy * p->drag * p->acceleration_multiplier;
        if(dx != 0. || dy != 0.) {
            alength = 1. / sqrt(ax * ax + ay * ay);
            dlength = 1. / sqrt(dx * dx + dy * dy);
            weight = .5 * ((ax * alength) * (dx * dlength)
                          + (ay * alength) * (dy * dlength) + 1.);
            dx *= weight;
            dy *= weight;
            if(c->stop) {
                double vnormal = s->vx * ux + s->vy * uy;
                double anormal = dx * ux + dy * uy;
                if((anormal > 0.) != (vnormal > 0.) && fabs(anormal) > fabs(vnormal)) {
                    dx = -vnormal * ux;
                    dy = -vnormal * uy;
                }
            }
            s->vx += dx;
            s->vy += dy;
        }
    }
    s->x += s->vx;
    s->y += s->vy;
}
