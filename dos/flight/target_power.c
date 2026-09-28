/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "target_power.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int target_power_load(TargetPower *power,int stock,int stress)
{
    TargetPower parsed={0};
    unsigned char bytes[25];
    double values[2];unsigned i,j;
    FILE *file=fopen("BARGEHP.DAT","rb");
    if(!file) return 0;
    if(fread(bytes,1,sizeof(bytes),file)!=sizeof(bytes) || fgetc(file)!=EOF || ferror(file)) {
        fclose(file);return 0;
    }
    fclose(file);
    if(memcmp(bytes,"ESPLAYER1",9) || sizeof(double)!=8) return 0;
    for(i=0;i<2;++i) {
        uint64_t bits=0;
        for(j=0;j<8;++j) bits|=(uint64_t)bytes[9+8*i+j]<<(8*j);
        memcpy(&values[i],&bits,8);
    }
    if(!isfinite(values[0]) || values[0]<=0. || values[0]>1e9 ||
       !isfinite(values[1]) || values[1]<0. || values[1]>values[0] ||
       !resource_load(&parsed.budget,"BARGERES.DAT") ||
       !shield_load(&parsed.shield_profile,"BARGESH.DAT")) return 0;
    parsed.max_hull=values[0];parsed.minimum_hull=values[1];
    if(!stock) {
        /* Quick trainer keeps native recharge and scales only health/threshold. */
        parsed.minimum_hull*=26./parsed.max_hull;
        parsed.max_hull=26.;parsed.shield_profile.capacity=30.;
    }
    if(stress==1) { parsed.budget.capacity=40.;parsed.budget.generation=.25; }
    else if(stress==2) { parsed.budget.max_heat=450.;parsed.budget.heat_generation=0.; }
    parsed.enabled=1;parsed.stock=stock;parsed.stress=stress;
    *power=parsed;return 1;
}
void target_power_reset(TargetPower *power,DamageState *health)
{
    if(!power->enabled) return;
    resource_reset(&power->resources,&power->budget);
    shield_reset(&power->shield,&power->shield_profile);
    *health=(DamageState){power->shield.shields,power->max_hull};
    power->disabled=0;power->blocked_energy=power->blocked_heat=0;
}
void target_power_tick(TargetPower *power,DamageState *health)
{
    if(!power->enabled || health->hull<0.) return;
    power->disabled=health->hull<power->minimum_hull;
    power->shield.shields=health->shields;
    shield_tick(&power->shield,&power->shield_profile,&power->resources,
                power->disabled || power->resources.overheated);
    resource_tick_disabled(&power->resources,&power->budget,power->disabled);
    health->shields=power->shield.shields;
}
void target_power_hit(TargetPower *power,DamageState *health,double shields,double hull)
{
    double before=health->shields;
    int disabled=power->disabled || power->resources.overheated;
    damage_hit(health,shields,hull);
    if(!power->enabled) return;
    power->shield.shields=before;
    if(before>health->shields)
        shield_damage(&power->shield,&power->shield_profile,before-health->shields,disabled);
    power->shield.shields=health->shields;
    power->disabled=health->hull<power->minimum_hull;
}
int target_power_can_fire(TargetPower *power)
{
    if(!power->enabled) return 1;
    if(power->disabled) return 0;
    if(resource_can_fire(&power->resources,&power->budget)) return 1;
    if(power->resources.overheated) ++power->blocked_heat;
    else ++power->blocked_energy;
    return 0;
}
void target_power_fire(TargetPower *power)
{
    if(power->enabled) resource_fire(&power->resources,&power->budget);
}
