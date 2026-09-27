/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOS_PROJECTILE_H
#define DOS_PROJECTILE_H
#include "../collision/mask.h"
/* Healthy ordinary non-accelerating bolt. Caller resolves firing angle,
 * launch offset, reload/resources, allegiance and target eligibility. */
typedef struct {
    double x,y,vx,vy;
    int lifetime,alive;
    uint16_t angle;
} Bolt;
typedef struct {
    double x,y;
    uint16_t angle;
    const CollisionMask *mask;
} BoltTarget;
void bolt_launch(Bolt *b,double x,double y,double parent_vx,double parent_vy,
                 uint16_t angle,double speed,int lifetime);
void bolt_move(Bolt *b);
/* Native query interval is current position -> current position + velocity,
 * using target snapshots AFTER ship movement, not relative target velocity.
 * Returns target index or -1. Exact fraction1 is no hit; ties retain first. */
int bolt_hit(const Bolt *b,const BoltTarget *targets,unsigned count,double *fraction);
#endif
