/* Scripted incoming-fire trainer. Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "threat.h"
#include "../pursuit/pursuit.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int threat_load(Threat *threat,const char *path)
{
    unsigned char bytes[25];
    double values[2];
    FILE *file;
    unsigned i,j;
    if(!threat || !path || sizeof(double)!=8 || !(file=fopen(path,"rb"))) return 0;
    if(fread(bytes,1,sizeof(bytes),file)!=sizeof(bytes) || fgetc(file)!=EOF || ferror(file)) {
        fclose(file);return 0;
    }
    fclose(file);
    if(memcmp(bytes,"ESPLAYER1",9)) return 0;
    for(i=0;i<2;++i) {
        uint64_t bits=0;
        for(j=0;j<8;++j) bits|=(uint64_t)bytes[9+i*8+j]<<(j*8);
        memcpy(&values[i],&bits,8);
    }
    if(!isfinite(values[0]) || values[0]<=0. || values[0]>1e9 ||
       !isfinite(values[1]) || values[1]<0. || values[1]>values[0]) return 0;
    memset(threat,0,sizeof(*threat));
    threat->max_hull=values[0];threat->minimum_hull=values[1];threat->enabled=1;
    threat_reset(threat);
    return 1;
}

void threat_reset(Threat *threat)
{
    double max_hull=threat->max_hull,minimum_hull=threat->minimum_hull;
    int enabled=threat->enabled;
    memset(threat,0,sizeof(*threat));
    threat->max_hull=max_hull;threat->minimum_hull=minimum_hull;
    threat->hull=max_hull;threat->enabled=enabled;
}

void threat_hit(Threat *threat,Practice *practice)
{
    if(threat->destroyed) return;
    DamageState health={practice->shield.shields,threat->hull};
    double before=health.shields;
    int was_disabled=threat->disabled || practice->resources.overheated;
    ++threat->hits;threat->flash=8;
    damage_hit(&health,practice->shield_damage,practice->hull_damage);
    /* shield_damage owns the delay transition; its drain matches
     * damage_hit's actual shield loss, so no second drain occurs. */
    if(practice->shield_enabled && before>health.shields)
        shield_damage(&practice->shield,&practice->shield_profile,
                      before-health.shields,was_disabled);
    practice->shield.shields=health.shields;
    threat->hull=health.hull;
    threat->disabled=threat->hull<threat->minimum_hull;
    threat->destroyed=threat->hull<0.;
}

void threat_step(Threat *threat,Practice *practice,const Pilot *pilot)
{
    double source_vx=0.,source_vy=0.;
    unsigned i;
    double aim_x,aim_y;
    int can_reach=1;
    BoltTarget player;
    if(!threat || !threat->enabled || !practice || !pilot) return;
    ++threat->ticks;
    if(threat->flash) --threat->flash;
    if(threat->cooldown) --threat->cooldown;
    /* The target has already moved in practice_finish_tick. Derive its
     * velocity from successive post-move positions, not an AI prediction. */
    if(threat->have_previous_target && !practice->destroyed) {
        source_vx=practice->target.x-threat->previous_target_x;
        source_vy=practice->target.y-threat->previous_target_y;
    }
    if(practice->pursuit_target) {
        source_vx=practice->target_vx;
        source_vy=practice->target_vy;
    }
    threat->previous_target_x=practice->target.x;
    threat->previous_target_y=practice->target.y;
    threat->have_previous_target=1;
    for(i=0;i<THREAT_BOLTS;++i) bolt_move(&threat->bolts[i]);
    aim_x=pilot->state.x-practice->target.x;
    aim_y=pilot->state.y-practice->target.y;
    if(practice->pursuit_target) {
        double vx=pilot->state.vx-source_vx,vy=pilot->state.vy-source_vy;
        double time=pursuit_intercept(aim_x,aim_y,vx,vy,practice->speed);
        can_reach=isfinite(time) && time>=0. && time<=practice->lifetime;
        if(can_reach) { aim_x+=time*vx; aim_y+=time*vy; }
    }
    if(!practice->destroyed && !threat->destroyed && can_reach &&
       threat->ticks>=THREAT_GRACE_TICKS && !threat->cooldown &&
       target_power_can_fire(&practice->target_power)) {
        for(i=0;i<THREAT_BOLTS && threat->bolts[i].alive;++i) {}
        if(i<THREAT_BOLTS) {
            double ux,uy;
            uint16_t angle=motion_angle(atan2(aim_x,-aim_y)*
                                         (180./3.14159265358979323846));
            motion_unit(angle,&ux,&uy);
            bolt_launch(&threat->bolts[i],practice->target.x+20.*ux-.5*source_vx,
                        practice->target.y+20.*uy-.5*source_vy,
                        source_vx,source_vy,angle,practice->speed,practice->lifetime);
            ++threat->shots;
            target_power_fire(&practice->target_power);
        } else ++threat->dropped;
        threat->cooldown=(unsigned)practice->reload;
    }
    /* The existing bolt query uses the Sparrow mask at the player's current
     * pose and tests the projectile's next segment. Birth-tick hits are valid. */
    player=(BoltTarget){pilot->state.x,pilot->state.y,pilot->state.angle,&practice->masks[0]};
    threat->active=0;
    for(i=0;i<THREAT_BOLTS;++i) {
        Bolt *bolt=&threat->bolts[i];double fraction;
        if(!threat->destroyed && bolt_hit(bolt,&player,1,&fraction)>=0) {
            bolt->alive=0;threat_hit(threat,practice);
        }
        if(bolt->alive) ++threat->active;
    }
    if(threat->active>threat->peak) threat->peak=threat->active;
}

static void dot(unsigned char *frame,int x,int y,unsigned char color)
{
    if(x>=0 && x<800 && y>=40 && y<570) frame[y*800+x]=color;
}
void threat_draw(unsigned char *frame,const Threat *threat,const Scene *scene)
{
    unsigned i;
    for(i=0;i<THREAT_BOLTS;++i) if(threat->bolts[i].alive) {
        const Bolt *b=&threat->bolts[i];double ux,uy;int k;
        double x=b->x+.5*b->vx-scene->camera_x;
        double y=b->y+.5*b->vy-scene->camera_y;
        if(x< -8. || x>808. || y<32. || y>578.) continue;
        motion_unit(b->angle,&ux,&uy);
        for(k=-3;k<=3;++k)
            dot(frame,(int)lround(x+k*ux),(int)lround(y+k*uy),k?6:4);
    }
}
