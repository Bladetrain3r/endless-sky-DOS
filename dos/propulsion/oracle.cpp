/* Actual Ship::Move and optional Ship::ExpendAmmo reference, unchanged linked objects.
 * Copyright (c) 2026 Endless Sky DOS contributors.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <bits/stdc++.h>
#define private public
#define protected public
#include "Ship.h"
#undef private
#undef protected
#include "Command.h"
#include "Files.h"
#include "Flotsam.h"
#include "GameData.h"
#include "Outfit.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "System.h"
#include "TaskQueue.h"
#include "Visual.h"
#include "Weapon.h"
using namespace std;

struct Case {
    const char *name;
    int ticks;
    double capacity, generation, maxHeat, startEnergy, startHeat, vx, vy;
    int initialOverheated, shotPeriod;
};
static const Case CASES[] = {
    {"stock_forward", 90, -1., -1., -1., -1., 0., 0., 0., 0, 0},
    {"tiny_forward", 35, .5, .25, -1., 0., 0., 1., -.5, 0, 0},
    {"tiny_turn", 35, .5, .25, -1., 0., 0., 1., -.5, 0, 0},
    {"tiny_both", 35, .5, .25, -1., 0., 0., 1., -.5, 0, 0},
    {"negative_shared", 35, .75, .25, -1., .5, 0., 1., -.5, 0, 0},
    {"negative_turn", 15, .5, .25, -1., 0., 0., 1., -.5, 0, 0},
    {"zero_capacity", 15, 0., 0., -1., 0., 0., 2., -1., 0, 0},
    {"backward", 18, -1., -1., -1., -1., 0., 1., -.5, 0, 0},
    {"cancel", 18, -1., -1., -1., -1., 0., 1., -.5, 0, 0},
    {"coast", 18, -1., -1., -1., -1., 0., 1., -.5, 0, 0},
    {"heat_lock", 28, -1., -1., 450., -1., 451., 2., -1., 1, 0},
    {"heat_recovery", 180, -1., -1., 450., -1., 451., 2., -1., 1, 0},
    {"needs_energy", 15, .5, 0., -1., 0., 0., 2., -1., 0, 0},
    {"needs_energy_boundary", 15, .5, 0., -1., .00390625, 0., 2., -1., 0, 0},
    {"needs_energy_above", 15, .5, 0., -1., .0039062501, 0., 2., -1., 0, 0},
    {"energy_shots", 50, 10., .25, -1., 10., 0., 1., -.5, 0, 1},
    {"heat_shots", 50, -1., -1., 450., -1., 390., 1., -.5, 0, 1}
};

static tuple<int,int,int,double> Input(const string &name, int tick)
{
    if(name == "tiny_turn") return {0,0,0,1.};
    if(name == "negative_turn") return {0,0,0,-1.};
    if(name == "backward") return {0,1,0,0.};
    if(name == "cancel") return {1,1,0,0.};
    if(name == "coast") return {0,0,0,0.};
    if(name == "tiny_both" || name == "energy_shots" || name == "heat_shots"
        || name == "negative_shared") return {1,0,0,name == "negative_shared" ? -.5 : 1.};
    if(name == "heat_recovery") return {1,0,0,tick < 145 ? 1. : -1.};
    return {1,0,0,0.};
}

int main(int argc, char **argv)
{
    try {
        if(argc < 3) throw runtime_error("Expected CSV and propulsion profile paths");
        Files::Init(const_cast<const char *const *>(argv));
        Preferences::Load(); PluginManager::LoadSettings();
        TaskQueue::SetWorkerThreadCount(1);
        TaskQueue queue; PlayerInfo player;
        GameData::BeginLoad(queue, player, true, false, true).get();
        const Ship *model = GameData::Ships().Get("Sparrow");
        const System *system = GameData::Systems().Get("Sol");
        const Outfit *outfit = GameData::Outfits().Get("Energy Blaster");
        if(!model || !model->IsValid() || !system || !system->IsValid() || !outfit || !outfit->GetWeapon())
            throw runtime_error("Missing stock fixture");
        const Weapon &weapon = *outfit->GetWeapon();
        Ship stock = *model;
        stock.crew = max(1, stock.RequiredCrew());
        stock.levels.hull = stock.MaxHull(); stock.levels.shields = stock.MaxShields();
        stock.SetSystem(system);
        stock.Recharge(Port::RechargeType::All, true);
        if(stock.ReverseAcceleration() || stock.cache.afterburnerThrust || stock.cache.activeCooling
            || stock.cache.fuelEnergy || stock.cache.fuelConsumption || stock.cache.solarCollection
            || stock.cache.inertiaReduction != 1. || stock.cache.accelerationMult != 1.)
            throw runtime_error("Unsupported stock propulsion modifier");
        auto checkCost = [&](const ResourceLevels &c) {
            if(c.hull || c.shields || c.fuel || c.corrosion || c.discharge || c.ionization
                || c.burning || c.leakage || c.scrambling || c.disruption || c.slowness
                || c.energy < 0. || c.heat < 0.) throw runtime_error("Unsupported movement cost");
        };
        checkCost(stock.cache.thrustCost); checkCost(stock.cache.turnCost);
        const double generation = stock.cache.energyGeneration - stock.cache.energyConsumption;
        ofstream profile(argv[2]);
        profile << setprecision(17) << "ESPROPULSION1\n"
            << stock.cache.thrustCost.energy << ' ' << stock.cache.thrustCost.heat << ' '
            << stock.cache.turnCost.energy << ' ' << stock.cache.turnCost.heat << '\n';
        ofstream out(argv[1]); out << setprecision(17);
        if(!out || !profile) throw runtime_error("Output open failed");
        out << "case,tick,forward,back,stop,turn,shot,accel,reverse_accel,turn_rate,drag,mult,capacity,generation,heat_generation,dissipation,max_heat,shot_energy,shot_heat,start_energy,start_heat,start_overheated,x,y,vx,vy,angle,energy,heat,overheated,fired\n";
        int rows = 0;
        for(const Case &test : CASES) {
            Ship ship = stock;
            ship.capacities.energy = test.capacity >= 0. ? test.capacity : stock.MaxEnergy();
            ship.cache.energyGeneration = (test.generation >= 0. ? test.generation : generation) + ship.cache.energyConsumption;
            if(test.maxHeat >= 0.) {
                ship.cache.heatCapacity += (test.maxHeat - stock.MaxHeat()) / 100.;
                ship.cache.heatGeneration = ship.CoolingEfficiency() * ship.cache.cooling;
            }
            ship.levels.energy = test.startEnergy >= 0. ? test.startEnergy : ship.MaxEnergy();
            ship.levels.heat = test.startHeat;
            ship.isOverheated = ship.isDisabled = test.initialOverheated;
            ship.Place(Point(125., -73.), Point(test.vx, test.vy), Angle(0.), true);
            /* Place restores IdleHeat; test the explicit fixture level instead. */
            ship.levels.heat = test.startHeat;
            vector<Visual> visuals; list<shared_ptr<Flotsam>> flotsam;
            for(int tick = 0; tick <= test.ticks; ++tick) {
                const auto [forward, back, stop, turn] = Input(test.name, tick);
                int shot = tick && test.shotPeriod && (tick % test.shotPeriod == 0);
                int fired = 0;
                if(tick) {
                    Command command;
                    if(forward) command.Set(Command::FORWARD);
                    if(back) command.Set(Command::BACK);
                    if(stop) command.Set(Command::STOP);
                    command.SetTurn(turn);
                    ship.SetCommands(command);
                    ship.Move(visuals, flotsam);
                    if(shot && !ship.isDisabled && ship.CanFire(&weapon) == Ship::CanFireResult::CAN_FIRE) {
                        ship.ExpendAmmo(weapon);
                        fired = 1;
                    }
                    visuals.clear(); flotsam.clear();
                }
                int angle = int(llround(ship.Facing().AbsDegrees() * 65536. / 360.)) & 65535;
                out << test.name << ',' << tick << ',' << forward << ',' << back << ',' << stop << ',' << turn
                    << ',' << shot << ',' << ship.Acceleration() << ',' << ship.ReverseAcceleration()
                    << ',' << ship.TurnRate() << ',' << ship.DragForce() << ',' << ship.cache.accelerationMult
                    << ',' << ship.MaxEnergy() << ',' << ship.cache.energyGeneration - ship.cache.energyConsumption
                    << ',' << ship.cache.heatGeneration - ship.CoolingEfficiency() * ship.cache.cooling
                    << ',' << ship.HeatDissipation() << ',' << ship.MaxHeat() << ','
                    << weapon.FiringEnergy() << ',' << weapon.FiringHeat() << ','
                    << (test.startEnergy >= 0. ? test.startEnergy : ship.MaxEnergy())
                    << ',' << test.startHeat << ',' << test.initialOverheated << ','
                    << ship.Position().X() << ',' << ship.Position().Y() << ','
                    << ship.Velocity().X() << ',' << ship.Velocity().Y() << ',' << angle << ','
                    << ship.EnergyLevel() << ',' << ship.HeatLevel() << ',' << ship.IsOverheated()
                    << ',' << fired << '\n';
                ++rows;
            }
        }
        if(!out.good() || !profile.good()) throw runtime_error("Output write failed");
        cout << setprecision(17) << "cases=" << size(CASES) << " rows=" << rows
            << " thrust_energy=" << stock.cache.thrustCost.energy
            << " thrust_heat=" << stock.cache.thrustCost.heat
            << " turn_energy=" << stock.cache.turnCost.energy
            << " turn_heat=" << stock.cache.turnCost.heat << '\n';
    } catch(const exception &e) { cerr << "Propulsion oracle failed: " << e.what() << '\n'; return 1; }
}
