/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "projectile.h"
#include "../motion/motion.h"
void bolt_launch(Bolt *b,double x,double y,double parent_vx,double parent_vy,
                 uint16_t angle,double speed,int lifetime)
{
    double ux,uy;
    motion_unit(angle,&ux,&uy);
    *b=(Bolt){x,y,parent_vx+speed*ux,parent_vy+speed*uy,lifetime,1,angle};
}
void bolt_move(Bolt *b)
{
    if(!b->alive) return;
    if(--b->lifetime<=0) { b->alive=0; return; }
    b->x+=b->vx; b->y+=b->vy;
}
int bolt_hit(const Bolt *b,const BoltTarget *targets,unsigned count,double *fraction)
{
    unsigned i; int hit=-1; double first=1.;
    if(b->alive) for(i=0;i<count;++i) {
        const BoltTarget *t=&targets[i];
        double f=mask_collide(t->mask,(MaskPoint){b->x-t->x,b->y-t->y},
                             (MaskPoint){b->vx,b->vy},t->angle);
        if(f<first) { first=f; hit=(int)i; }
    }
    *fraction=first;
    return hit;
}
