/* Native Ship::DoGeneration shield oracle, linked with unchanged game objects.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <bits/stdc++.h>
#define private public
#define protected public
#include "Ship.h"
#undef private
#undef protected
#include "Files.h"
#include "GameData.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "TaskQueue.h"
using namespace std;

struct Scenario {
    const char *name; int ticks; double shields, energy, heat;
    int overheated, delay; double rate, cost; int damage_tick;
};
static const Scenario SCENARIOS[] = {
    {"stock_full", 6, -1., -1., 0., 0, 0, -1., -1., -1},
    {"stock_partial", 120, 700., -1., 0., 0, 0, -1., -1., -1},
    {"stock_depleted", 120, 0., -1., 0., 0, 0, -1., -1., -1},
    {"energy_limited", 100, 700., 0.1, 0., 0, 0, -1., -1., -1},
    {"one_tick_capacity", 5, 1399.9, -1., 0., 0, 0, -1., -1., -1},
    {"hot_prior_disabled", 16, 700., 3000., 16741., 1, 4, -1., -1., -1},
    {"hot_enters", 8, 700., 3000., 19000., 0, 0, -1., -1., -1},
    {"synthetic_delay", 8, 700., 3000., 0., 0, 4, -1., -1., -1},
    {"synthetic_heat", 20, 700., 3000., 0., 0, 0, 2., 1.5, -1},
    {"synthetic_damage", 20, -1., 3000., 0., 0, 0, -1., -1., 1}
};

int main(int argc, char **argv)
{
    try {
        if(argc < 3) throw runtime_error("Expected CSV and profile paths");
        Files::Init(const_cast<const char *const *>(argv));
        Preferences::Load(); PluginManager::LoadSettings();
        TaskQueue::SetWorkerThreadCount(1);
        TaskQueue queue; PlayerInfo player;
        GameData::BeginLoad(queue, player, true, false, true).get();
        const Ship *model = GameData::Ships().Get("Sparrow");
        if(!model || !model->IsValid()) throw runtime_error("No stock Sparrow");
        Ship stock = *model;
        stock.crew = max(1, stock.RequiredCrew());
        stock.levels.hull = stock.MaxHull(); stock.levels.shields = stock.MaxShields();
        const auto &c = stock.cache;
        if(c.hullRepairRate || c.hullRepairRateWithDelay || c.shieldRegenCost.fuel ||
           c.shieldRegenWithDelayCost.fuel || c.shieldRegenCost.hull ||
           c.shieldRegenWithDelayCost.hull || c.shieldRegenCost.heat < 0. ||
           c.shieldRegenWithDelayCost.heat < 0. || c.cloakedRegenMult ||
           c.activeCooling || c.fuelConsumption || c.fuelEnergy || c.fuelHeat ||
           c.recoveryTime || c.overheatDamageRate || !stock.bays.empty())
            throw runtime_error("Unsupported stock repair/generation attribute");
        // Verify disable threshold and generation against unchanged Ship code.
        Ship disabled = stock;
        disabled.levels.hull = stock.minimumHull - 1.;
        disabled.levels.shields = 700.; disabled.levels.energy = 100.;
        disabled.levels.heat = 0.; disabled.isOverheated = false; disabled.isDisabled = true;
        disabled.DoGeneration();
        if(!disabled.isDisabled || disabled.levels.energy != 100. || disabled.levels.shields != 700.)
            throw runtime_error("Unexpected hull-disabled generation/repair");
        disabled.levels.hull = stock.minimumHull;
        disabled.DoGeneration();
        if(disabled.isDisabled || disabled.levels.energy <= 100. || disabled.levels.shields != 700.)
            throw runtime_error("Unexpected hull threshold equality semantics");
        ofstream profile(argv[2]); if(!profile) throw runtime_error("Profile open failed");
        profile << setprecision(17) << "ESSHIELD1\n" << stock.MaxShields() << ' '
            << c.shieldRegenRate << ' ' << c.shieldRegenCost.energy << ' '
            << c.shieldRegenCost.heat << ' ' << c.shieldRegenRateWithDelay << ' '
            << c.shieldRegenWithDelayCost.energy << ' ' << c.shieldRegenWithDelayCost.heat
            << ' ' << c.shieldDelay << ' ' << c.depletedShieldDelay << '\n';
        ofstream out(argv[1]); if(!out) throw runtime_error("CSV open failed");
        out << setprecision(17)
            << "case,tick,capacity,rate,energy_cost,heat_cost,delayed_rate,delayed_energy_cost,delayed_heat_cost,profile_delay,depleted_delay,initial_shields,initial_energy,initial_heat,initial_overheated,initial_delay,damage,prior_disabled,shields,energy,heat,overheated,delay\n";
        int rows = 0;
        for(const auto &test : SCENARIOS) {
            Ship ship = stock;
            if(test.rate >= 0.) {
                ship.cache.shieldRegenRate = test.rate;
                ship.cache.shieldRegenCost.energy = test.cost;
                ship.cache.shieldRegenCost.heat = .125;
            }
            if(string(test.name) == "synthetic_delay") {
                ship.cache.shieldRegenRateWithDelay = .05;
                ship.cache.shieldRegenWithDelayCost.energy = .4;
            }
            if(string(test.name) == "synthetic_damage") {
                ship.cache.shieldDelay = 3;
                ship.cache.depletedShieldDelay = 5;
            }
            ship.levels.shields = test.shields < 0. ? ship.MaxShields() : test.shields;
            ship.levels.energy = test.energy < 0. ? ship.MaxEnergy() : test.energy;
            ship.levels.heat = test.heat;
            ship.isOverheated = ship.isDisabled = test.overheated;
            ship.shieldDelay = test.delay;
            double initialShields = ship.levels.shields;
            double initialEnergy = ship.levels.energy;
            double initialHeat = ship.levels.heat;
            for(int tick = 0; tick < test.ticks; ++tick) {
                double damage = tick == test.damage_tick ? 350. : 0.;
                int priorDisabled = ship.isDisabled;
                if(damage) {
                    ship.levels.shields = max(0., ship.levels.shields - damage);
                    if(!priorDisabled) {
                        int delay = ship.cache.depletedShieldDelay;
                        ship.shieldDelay = max(ship.shieldDelay,
                            (ship.levels.shields <= 0. && delay) ? delay : ship.cache.shieldDelay);
                    }
                }
                ship.DoGeneration();
                const auto &cache = ship.cache;
                out << test.name << ',' << tick << ',' << ship.MaxShields() << ','
                    << cache.shieldRegenRate << ',' << cache.shieldRegenCost.energy << ','
                    << cache.shieldRegenCost.heat << ',' << cache.shieldRegenRateWithDelay << ','
                    << cache.shieldRegenWithDelayCost.energy << ',' << cache.shieldRegenWithDelayCost.heat << ','
                    << cache.shieldDelay << ',' << cache.depletedShieldDelay << ','
                    << initialShields << ',' << initialEnergy << ',' << initialHeat << ','
                    << test.overheated << ',' << test.delay << ',' << damage << ',' << priorDisabled << ','
                    << ship.levels.shields << ',' << ship.levels.energy << ',' << ship.levels.heat << ','
                    << ship.isOverheated << ',' << ship.shieldDelay << '\n';
                ++rows;
            }
        }
        if(!out.good() || !profile.good()) throw runtime_error("Output write failed");
        cout << setprecision(17) << "cases=" << size(SCENARIOS) << " rows=" << rows
            << " shields=" << stock.MaxShields() << " rate=" << c.shieldRegenRate
            << " energy_cost=" << c.shieldRegenCost.energy << " heat_cost=" << c.shieldRegenCost.heat
            << " delayed_rate=" << c.shieldRegenRateWithDelay << " delayed_energy_cost="
            << c.shieldRegenWithDelayCost.energy << " delayed_heat_cost="
            << c.shieldRegenWithDelayCost.heat << " delay=" << c.shieldDelay
            << " max_hull=" << stock.MaxHull() << " minimum_hull=" << stock.minimumHull
            << " depleted_delay=" << c.depletedShieldDelay << '\n';
        return 0;
    } catch(const exception &e) { cerr << "Shield oracle failed: " << e.what() << '\n'; return 1; }
}
