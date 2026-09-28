/* Native Star Barge profile and bounded generation traces, linked with unchanged game objects.
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

struct Case { const char *name; int ticks; double shields, energy, heat, hull; int overheated; };

int main(int argc, char **argv)
{
    try {
        if(argc < 6) throw runtime_error("Expected CSV and four profile paths");
        Files::Init(const_cast<const char *const *>(argv));
        Preferences::Load(); PluginManager::LoadSettings();
        TaskQueue::SetWorkerThreadCount(1);
        TaskQueue queue; PlayerInfo player;
        GameData::BeginLoad(queue, player, true, false, true).get();
        const Ship *model = GameData::Ships().Get("Star Barge");
        const Outfit *outfit = GameData::Outfits().Get("Energy Blaster");
        if(!model || !model->IsValid() || !outfit || !outfit->GetWeapon())
            throw runtime_error("Stock Star Barge or Energy Blaster missing");
        Ship stock = *model;
        stock.crew = max(1, stock.RequiredCrew());
        stock.levels.hull = stock.MaxHull(); stock.levels.shields = stock.MaxShields();
        const auto &c = stock.cache;
        const Weapon &weapon = *outfit->GetWeapon();
        if(stock.Weapons().size() != 1 || !stock.Weapons().front().IsTurret()
            || stock.ReverseThrust() || stock.ReverseAcceleration() || stock.ShouldUseAfterburner()
            || c.afterburnerThrust || c.activeCooling || c.fuelConsumption || c.fuelEnergy
            || c.fuelHeat || c.fuelGeneration || c.ramscoop || c.solarCollection || c.solarHeat
            || c.hullRepairRate || c.hullRepairRateWithDelay
            || c.recoveryTime || c.overheatDamageRate || c.cloakedRegenMult
            || c.inertiaReduction != 1. || c.accelerationMult != 1. || !stock.bays.empty()
            || c.shieldRegenCost.fuel || c.shieldRegenCost.hull || c.shieldRegenCost.shields
            || c.shieldRegenWithDelayCost.fuel || c.shieldRegenWithDelayCost.hull
            || c.shieldRegenWithDelayCost.shields
            || c.shieldRegenCost.heat < 0. || c.shieldRegenWithDelayCost.heat < 0.
            || weapon.Ammo() || weapon.RelativeFiringEnergy() || weapon.RelativeFiringHeat()
            || weapon.FiringFuel() || weapon.FiringHull() || weapon.FiringShields())
            throw runtime_error("Unsupported stock fixture attribute");
        auto checkCost = [&](const ResourceLevels &v) {
            if(v.hull || v.shields || v.fuel || v.corrosion || v.discharge || v.ionization
                || v.burning || v.leakage || v.scrambling || v.disruption || v.slowness
                || v.energy < 0. || v.heat < 0.) throw runtime_error("Unsupported movement cost");
        };
        checkCost(c.thrustCost); checkCost(c.turnCost);
        auto checkRegen = [&](const ResourceLevels &v) {
            if(v.corrosion || v.discharge || v.ionization || v.burning || v.leakage
                || v.scrambling || v.disruption || v.slowness || v.energy < 0.)
                throw runtime_error("Unsupported shield regeneration cost");
        };
        checkRegen(c.shieldRegenCost); checkRegen(c.shieldRegenWithDelayCost);
        const double generation = c.energyGeneration - c.energyConsumption;
        const double heatGeneration = c.heatGeneration - stock.CoolingEfficiency() * c.cooling;
        auto write = [](const char *path, const char *header, initializer_list<double> values) {
            ofstream out(path); if(!out) throw runtime_error("Profile open failed");
            out << setprecision(17) << header << '\n';
            for(double value : values) out << value << ' ';
            out << '\n';
            if(!out.good()) throw runtime_error("Profile write failed");
        };
        write(argv[2], "ESRESOURCE1", {stock.MaxEnergy(), generation, heatGeneration,
            stock.HeatDissipation(), stock.MaxHeat(), weapon.FiringEnergy(), weapon.FiringHeat()});
        write(argv[3], "ESSHIELD1", {stock.MaxShields(), c.shieldRegenRate,
            c.shieldRegenCost.energy, c.shieldRegenCost.heat, c.shieldRegenRateWithDelay,
            c.shieldRegenWithDelayCost.energy, c.shieldRegenWithDelayCost.heat,
            double(c.shieldDelay), double(c.depletedShieldDelay)});
        write(argv[4], "ESPROPULSION1", {c.thrustCost.energy, c.thrustCost.heat,
            c.turnCost.energy, c.turnCost.heat});
        write(argv[5], "ESPLAYER1", {stock.MaxHull(), stock.minimumHull});
        Ship disabled = stock;
        disabled.levels.hull = stock.minimumHull - 1.;
        disabled.levels.energy = 100.; disabled.levels.shields = stock.MaxShields() / 2.;
        disabled.isDisabled = true;
        disabled.DoGeneration();
        if(!disabled.isDisabled || disabled.levels.energy != 100.
            || disabled.levels.shields != stock.MaxShields() / 2.)
            throw runtime_error("Unexpected disabled generation");
        disabled.levels.hull = stock.minimumHull;
        disabled.DoGeneration();
        if(disabled.isDisabled || disabled.levels.energy <= 100.)
            throw runtime_error("Unexpected minimum hull equality semantics");
        const Case tests[] = {
            {"healthy", 40, -1., -1., 0., -1., 0},
            {"shield_drain", 120, 0., -1., 0., -1., 0},
            {"energy_starved", 80, 0., 0., 0., -1., 0},
            {"overheat_recovery", 160, -1., -1., stock.MaxHeat() + 1., -1., 1},
            {"hull_disabled", 30, 0., 100., 0., stock.minimumHull - 1., 0}
        };
        ofstream trace(argv[1]); if(!trace) throw runtime_error("Trace open failed");
        trace << setprecision(17) << "case,tick,prior_disabled,shields,energy,heat,overheated,delay,hull,disabled\n";
        int rows = 0;
        for(const Case &test : tests) {
            Ship ship = stock;
            ship.levels.shields = test.shields < 0. ? stock.MaxShields() : test.shields;
            ship.levels.energy = test.energy < 0. ? stock.MaxEnergy() : test.energy;
            ship.levels.heat = test.heat;
            ship.levels.hull = test.hull < 0. ? stock.MaxHull() : test.hull;
            ship.isOverheated = test.overheated;
            ship.isDisabled = ship.levels.hull < stock.minimumHull || test.overheated;
            for(int tick = 0; tick < test.ticks; ++tick) {
                int priorDisabled = ship.isDisabled;
                ship.DoGeneration();
                trace << test.name << ',' << tick << ',' << priorDisabled << ','
                    << ship.levels.shields << ',' << ship.levels.energy << ',' << ship.levels.heat << ','
                    << ship.isOverheated << ',' << ship.shieldDelay << ',' << ship.levels.hull << ','
                    << ship.isDisabled << '\n';
                ++rows;
            }
        }
        if(!trace.good()) throw runtime_error("Trace write failed");
        cout << setprecision(17) << "cases=" << size(tests) << " rows=" << rows
            << " max_hull=" << stock.MaxHull() << " minimum_hull=" << stock.minimumHull << '\n';
    } catch(const exception &e) { cerr << "Opponent profile oracle failed: " << e.what() << '\n'; return 1; }
}
