/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "opponent.h"
#include "threat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"lifecycle line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
static void reset(Practice *p,Opponent *enemy,Threat *threat,Pilot *pilot)
{
    practice_reset(p);opponent_reset(enemy,p);threat_reset(threat);
    enemy->state=(MotionState){0};
    pilot->state=(MotionState){0.,-150.,0.,0.,0};
    p->target=(BoltTarget){0.,0.,0,&p->masks[1]};
    p->target_vx=p->target_vy=0.;
    threat->ticks=THREAT_GRACE_TICKS-1;
}
void target_power_tests(Practice *p,Opponent *enemy,Threat *threat,Pilot *pilot)
{
    TargetPower *tp=&p->target_power;
    unsigned i;
    CHECK(target_power_load(tp,0,0));
    reset(p,enemy,threat,pilot);
    CHECK(p->health.shields==30. && p->health.hull==26.);
    CHECK(tp->minimum_hull>0. && tp->minimum_hull<26.);
    /* Fire debits exactly one shot; cooldown attempts debit nothing. */
    {
        double energy=tp->resources.energy,heat=tp->resources.heat;
        threat_step(threat,p,pilot);
        CHECK(threat->shots==1 && tp->resources.energy==energy-tp->budget.shot_energy);
        CHECK(tp->resources.heat==heat+tp->budget.shot_heat);
        energy=tp->resources.energy;threat_step(threat,p,pilot);
        CHECK(threat->shots==1 && tp->resources.energy==energy);
    }
    /* Energy denial does not spend heat or start a reload. */
    reset(p,enemy,threat,pilot);tp->resources.energy=0.;
    threat_step(threat,p,pilot);
    CHECK(!threat->shots && !threat->cooldown && tp->blocked_energy==1 && tp->resources.heat==0.);
    /* A full pool also spends nothing, independent of fire eligibility. */
    reset(p,enemy,threat,pilot);
    for(i=0;i<THREAT_BOLTS;++i)
        bolt_launch(&threat->bolts[i],5000.,5000.,0.,0.,0,p->speed,p->lifetime);
    threat_step(threat,p,pilot);
    CHECK(!threat->shots && threat->dropped==1 && tp->resources.energy==tp->budget.capacity);
    /* Recharge uses prior energy. No energy at entry means this tick cannot
     * repair even though generation replenishes the battery afterwards. */
    reset(p,enemy,threat,pilot);p->health.shields=10.;tp->resources.energy=0.;
    target_power_tick(tp,&p->health);
    CHECK(p->health.shields==10. && tp->resources.energy==tp->budget.generation);
    target_power_tick(tp,&p->health);CHECK(p->health.shields>10. && p->health.shields<=30.);
    /* Prior overheat blocks repair on the recovery tick; propulsion/fire may
     * recover after generation clears overheat, as in the player pipeline. */
    tp->resources.overheated=1;tp->resources.heat=0.;
    {
        double shields=p->health.shields;
        target_power_tick(tp,&p->health);
        CHECK(!tp->resources.overheated && p->health.shields==shields);
        target_power_tick(tp,&p->health);CHECK(p->health.shields>shields);
    }
    /* Overheat blocks steering, thrust and firing, but integrates native drag. */
    reset(p,enemy,threat,pilot);enemy->state.vx=2.;
    tp->resources.heat=tp->budget.max_heat*1.1;
    opponent_step(enemy,p,pilot,0);
    CHECK(tp->resources.overheated && enemy->state.angle==0);
    CHECK(fabs(enemy->state.vx-2.*(1.-enemy->parameters.drag))<1e-12);
    threat_step(threat,p,pilot);CHECK(!threat->shots && tp->blocked_heat==1);
    /* The boundary itself is healthy; the first below-threshold hit disables
     * before that tick's hostile fire. Already launched shots survive. */
    reset(p,enemy,threat,pilot);p->health.shields=0.;p->health.hull=tp->minimum_hull;
    target_power_tick(tp,&p->health);CHECK(!tp->disabled);
    p->health.shields=0.;
    target_power_hit(tp,&p->health,0.,.01);
    CHECK(tp->disabled && p->health.hull>0.);
    bolt_launch(&threat->bolts[0],5000.,5000.,0.,0.,0,p->speed,p->lifetime);
    threat_step(threat,p,pilot);
    CHECK(!threat->shots && threat->bolts[0].alive);
    {
        double energy=tp->resources.energy,shields=p->health.shields;
        if(energy>tp->budget.capacity) energy=tp->budget.capacity;
        enemy->state.vx=2.;
        opponent_step(enemy,p,pilot,0);
        CHECK(tp->resources.energy==energy && p->health.shields==shields);
        CHECK(enemy->state.angle==0 && fabs(enemy->state.vx-2.*(1.-enemy->parameters.drag))<1e-12);
    }
    /* Shield delay uses the hit transition once, without double damage. */
    reset(p,enemy,threat,pilot);tp->shield_profile.delay=12;
    target_power_hit(tp,&p->health,5.,5.);
    CHECK(p->health.shields==25. && tp->shield.shields==25. && tp->shield.delay==12);
    CHECK(p->health.hull==26.);
    /* Destruction remains strict hull<0, and reset clears all lifecycle state. */
    p->health.shields=0.;p->health.hull=0.;
    target_power_tick(tp,&p->health);CHECK(tp->disabled);
    target_power_hit(tp,&p->health,0.,1.);CHECK(p->health.hull<0.);
    p->destroyed=1;
    {
        double energy=tp->resources.energy;unsigned ticks=enemy->ticks;
        opponent_step(enemy,p,pilot,0);
        CHECK(enemy->ticks==ticks && tp->resources.energy==energy);
    }
    reset(p,enemy,threat,pilot);
    CHECK(!p->destroyed && !tp->disabled && !tp->resources.overheated);
    CHECK(tp->resources.energy==tp->budget.capacity && tp->resources.heat==0.);
    CHECK(!tp->blocked_energy && !tp->blocked_heat && !tp->shield.delay);
    CHECK(!threat->active && !threat->shots);
    /* Exercise the real friendly collision pass, not just the damage helper:
     * it must publish disable before threat_step's same-tick firing decision. */
    p->destructible=1;p->health.shields=0.;p->health.hull=tp->minimum_hull;
    bolt_launch(&p->bolts[0],p->target.x,p->target.y,0.,0.,0,p->speed,p->lifetime);
    practice_finish_tick(p,pilot,0);
    CHECK(p->hits==1 && tp->disabled && !p->destroyed);
    threat_step(threat,p,pilot);CHECK(!threat->shots);
    /* Player budgets never alias the target's resource state. */
    p->resources.energy=123.;p->resources.heat=456.;
    target_power_tick(tp,&p->health);
    CHECK(p->resources.energy==123. && p->resources.heat==456.);
    CHECK(target_power_load(tp,1,0));reset(p,enemy,threat,pilot);
    CHECK(p->health.hull>26. && p->health.shields>30. && tp->stock);
    CHECK(target_power_load(tp,0,1));reset(p,enemy,threat,pilot);
    CHECK(tp->budget.capacity==40. && tp->budget.generation==.25);
    CHECK(target_power_load(tp,0,2));reset(p,enemy,threat,pilot);
    CHECK(tp->budget.max_heat==450. && tp->budget.heat_generation==0.);
    puts("opponent_lifecycle=pass\nshot_budget=pass\nrecharge_order=pass\nthermal_drift=pass\nsame_tick_disable=pass\nhealth_presets=pass");
}
