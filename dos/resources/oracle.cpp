/* Native Ship::DoGeneration and Ship::ExpendAmmo resource oracle.
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
#include "Outfit.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "TaskQueue.h"
#include "Weapon.h"
using namespace std;

struct Scenario { const char *name; int ticks; double capacity, generation, maxHeat; int period; double initialEnergy, initialHeat; int initialOverheated; };
static const Scenario SCENARIOS[] = {
    {"stock_sustained", 240, -1., -1., -1., 12, -1., 0., 0},
    {"stock_burst", 20, -1., -1., -1., 1, -1., 0., 0},
    {"energy_starve", 160, 40., .25, -1., 1, -1., 0., 0},
    {"energy_release", 140, 40., .25, -1., 12, 0., 0., 0},
    {"heat_overheat", 180, -1., -1., 450., 1, -1., 0., 0},
    {"heat_recovery", 180, -1., -1., 450., 0, -1., 451., 1},
    {"heat_exact_max", 2, -1., -1., 450., 0, -1., 450., 0},
    {"heat_exact_recover", 2, -1., -1., 450., 0, -1., 405., 1},
    {"energy_overflow", 3, 40., .25, -1., 0, 80., 0., 0}
};

int main(int argc, char **argv)
{
    try {
        if(argc < 3) throw runtime_error("Expected CSV and profile output paths");
        Files::Init(const_cast<const char *const *>(argv));
        Preferences::Load(); PluginManager::LoadSettings();
        TaskQueue::SetWorkerThreadCount(1);
        TaskQueue queue; PlayerInfo player;
        GameData::BeginLoad(queue, player, true, false, true).get();
        const Ship *model = GameData::Ships().Get("Sparrow");
        const Outfit *outfit = GameData::Outfits().Get("Energy Blaster");
        if(!model || !model->IsValid() || !outfit || !outfit->GetWeapon())
            throw runtime_error("Sparrow or Energy Blaster unavailable");
        const Weapon &weapon = *outfit->GetWeapon();
        Ship stock = *model;
        stock.crew = max(1, stock.RequiredCrew());
        stock.levels.hull = stock.MaxHull(); stock.levels.shields = stock.MaxShields();
        if(stock.cache.activeCooling || stock.cache.fuelConsumption || stock.cache.fuelEnergy ||
            stock.cache.fuelHeat || weapon.Ammo() || weapon.RelativeFiringEnergy() ||
            weapon.RelativeFiringHeat() || weapon.FiringFuel() || weapon.FiringHull() ||
            weapon.FiringShields())
            throw runtime_error("Unsupported stock passive resource modifier");
        double generation = stock.cache.energyGeneration - stock.cache.energyConsumption;
        double heatGeneration = stock.cache.heatGeneration - stock.CoolingEfficiency() * stock.cache.cooling;
        ofstream profile(argv[2]);
        if(!profile) throw runtime_error("Cannot open profile");
        profile << setprecision(17) << "ESRESOURCE1\n" << stock.MaxEnergy() << ' '
            << generation << ' ' << heatGeneration << ' ' << stock.HeatDissipation() << ' '
            << stock.MaxHeat() << ' ' << weapon.FiringEnergy() << ' ' << weapon.FiringHeat() << '\n';
        ofstream out(argv[1]); if(!out) throw runtime_error("Cannot open CSV");
        out << setprecision(17);
        out << "case,tick,capacity,generation,heat_generation,dissipation,max_heat,shot_energy,shot_heat,initial_energy,initial_heat,initial_overheated,request,can_fire,fired,energy,heat,overheated\n";
        int rows = 0;
        for(const auto &test : SCENARIOS) {
            Ship ship = stock;
            ship.capacities.energy = test.capacity >= 0. ? test.capacity : stock.MaxEnergy();
            ship.cache.energyGeneration = (test.generation >= 0. ? test.generation : generation)
                + ship.cache.energyConsumption;
            if(test.maxHeat >= 0.) {
                ship.cache.heatCapacity += (test.maxHeat - stock.MaxHeat()) / 100.;
                ship.cache.heatGeneration = ship.CoolingEfficiency() * ship.cache.cooling;
            }
            if(string(test.name).find("heat_exact") == 0) ship.heatDissipation = 0.;
            ship.levels.energy = test.initialEnergy >= 0. ? test.initialEnergy : ship.MaxEnergy();
            ship.levels.heat = test.initialHeat;
            ship.isOverheated = ship.isDisabled = test.initialOverheated;
            for(int tick = 0; tick < test.ticks; ++tick) {
                ship.DoGeneration();
                bool request = test.period > 0 && tick % test.period == 0;
                bool can = !ship.isDisabled && ship.CanFire(&weapon) == Ship::CanFireResult::CAN_FIRE;
                bool fired = request && can;
                if(fired) ship.ExpendAmmo(weapon);
                out << test.name << ',' << tick << ',' << ship.MaxEnergy() << ','
                    << ship.cache.energyGeneration - ship.cache.energyConsumption << ','
                    << ship.cache.heatGeneration - ship.CoolingEfficiency() * ship.cache.cooling << ',' << ship.HeatDissipation() << ',' << ship.MaxHeat() << ','
                    << weapon.FiringEnergy() << ',' << weapon.FiringHeat() << ','
                    << (test.initialEnergy >= 0. ? test.initialEnergy : ship.MaxEnergy()) << ','
                    << test.initialHeat << ',' << test.initialOverheated << ',' << request << ','
                    << can << ',' << fired << ',' << ship.EnergyLevel() << ','
                    << ship.HeatLevel() << ',' << ship.IsOverheated() << '\n';
                ++rows;
            }
        }
        if(!out.good() || !profile.good()) throw runtime_error("Output write failed");
        cout << setprecision(17) << "cases=" << size(SCENARIOS) << " rows=" << rows
            << " capacity=" << stock.MaxEnergy() << " generation=" << generation
            << " heat_generation=" << heatGeneration << " dissipation=" << stock.HeatDissipation()
            << " max_heat=" << stock.MaxHeat() << " shot_energy=" << weapon.FiringEnergy()
            << " shot_heat=" << weapon.FiringHeat() << '\n';
        return 0;
    } catch(const exception &e) { cerr << "Resource oracle failed: " << e.what() << '\n'; return 1; }
}
