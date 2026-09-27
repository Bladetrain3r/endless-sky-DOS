/* Native Ship::Move movement oracle.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later

Linked against unchanged upstream objects. No private access or class changes.
*/

#include "Angle.h"
#include "Command.h"
#include "Files.h"
#include "Flotsam.h"
#include "GameData.h"
#include "Outfit.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "Ship.h"
#include "System.h"
#include "TaskQueue.h"
#include "Visual.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
constexpr int TICKS = 900;
double minimumEnergyMargin = numeric_limits<double>::infinity();
double minimumHeatMargin = numeric_limits<double>::infinity();

struct Controls {
	bool forward = false;
	bool back = false;
	bool stop = false;
	double turn = 0.;
};

Controls Input(const string &scenario, int tick)
{
	if(scenario == "forward") return {true, false, false, 0.};
	if(scenario == "coast") return {};
	if(scenario == "turn") return {true, false, false, 1.};
	if(scenario == "back") return {false, true, false, 0.};
	if(scenario == "stop") return {true, false, true, 0.};
	if(scenario == "mixed" || scenario == "pilot")
	{
		if(tick <= 120) return {true, false, false, 0.};
		if(tick <= 240) return {};
		if(tick <= 360) return {true, false, false, scenario == "pilot" ? -1. : -.5};
		if(tick <= 480) return {false, true, false, 0.};
		if(tick <= 600) return {true, false, false, scenario == "pilot" ? 1. : .25};
		if(tick <= 720) return {true, true, false, 0.};
		return {};
	}
	throw runtime_error("Unknown scenario");
}

void Check(const Ship &ship, const string &name, int tick)
{
	if(ship.IsDisabled() || ship.IsDestroyed() || ship.NeedsEnergy() || ship.Crew() < ship.RequiredCrew())
		throw runtime_error(name + " cannot move normally at tick " + to_string(tick));
	if(ship.EnergyLevel() < 0. || ship.FuelLevel() < 0. || ship.HullLevel() <= 0.)
		throw runtime_error(name + " depleted resources at tick " + to_string(tick));
	if(!isfinite(ship.Position().X()) || !isfinite(ship.Position().Y())
		|| !isfinite(ship.Velocity().X()) || !isfinite(ship.Velocity().Y()))
		throw runtime_error(name + " non-finite state at tick " + to_string(tick));
}

void CheckThrottle(const Ship &ship, const string &name, int tick, Controls input)
{
	const Outfit &attr = ship.Attributes();
	if(attr.Get("thrust") <= 0. || attr.Get("afterburner thrust") != 0.
		|| attr.Get("acceleration multiplier") != 0. || attr.Get("inertia reduction") != 0.)
		throw runtime_error(name + " has unsupported propulsion attributes");
	// The selected vanilla fittings have only energy and heat movement costs.
	// Reject a future data change instead of silently tracing fractional throttle.
	for(const string prefix : {"thrusting ", "turning ", "reverse thrusting "})
		for(const string resource : {"hull", "shields", "fuel", "corrosion", "discharge", "ion",
			"scramble", "burn", "leakage", "disruption", "slowing"})
			if(attr.Get(prefix + resource) != 0.)
				throw runtime_error(name + " has unsupported movement cost: " + prefix + resource);
	ResourceLevels required;
	const double turn = fabs(input.turn);
	const string thrust = input.forward != input.back ? (input.forward ? "thrusting " : "reverse thrusting ") : "";
	required.energy = turn * attr.Get("turning energy") + (thrust.empty() ? 0. : attr.Get(thrust + "energy"));
	required.heat = turn * attr.Get("turning heat") + (thrust.empty() ? 0. : attr.Get(thrust + "heat"));
	const ResourceLevels available = ship.AvailableResources();
	minimumEnergyMargin = min(minimumEnergyMargin, available.energy - required.energy);
	minimumHeatMargin = min(minimumHeatMargin, available.heat + required.heat);
	if(!available.CanExpend(required))
		throw runtime_error(name + " lacks full movement throttle at tick " + to_string(tick));
}

void Row(ostream &out, const Ship &ship, const string &name, int tick, Controls input)
{
	const double multiplier = 1. + ship.Attributes().Get("acceleration multiplier");
	const auto angleSteps = static_cast<int>(llround(ship.Facing().AbsDegrees() * 65536. / 360.)) & 65535;
	out << name << ',' << tick << ',' << input.forward << ',' << input.back << ',' << input.stop << ','
		<< input.turn << ',' << ship.Acceleration() << ',' << ship.ReverseAcceleration() << ','
		<< ship.TurnRate() << ',' << ship.DragForce() << ',' << multiplier << ','
		<< ship.Position().X() << ',' << ship.Position().Y() << ','
		<< ship.Velocity().X() << ',' << ship.Velocity().Y() << ',' << angleSteps << '\n';
}

void Run(ostream &out, const Ship &model, const System *system, const string &caseName,
	const string &scenario, const Outfit *reverse)
{
	auto ship = make_shared<Ship>(model);
	if(reverse)
	{
		if(ship->AddOutfit(reverse, 1) != 1 || ship->ReverseAcceleration() <= 0.)
			throw runtime_error("Reverse fixture not installed");
	}
	ship->SetSystem(system);
	ship->Recharge(Port::RechargeType::All, true);
	Point pos(125., -73.);
	Point velocity(0., 0.);
	Angle angle(0.);
	if(scenario == "coast") velocity = Point(2., -3.);
	if(scenario == "back") velocity = Point(1., -1.);
	if(scenario == "stop") velocity = Point(.3, .01);
	if(scenario == "mixed")
	{
		velocity = Point(1.25, -.75);
		angle = Angle(359.);
	}
	ship->Place(pos, velocity, angle, true);
	Check(*ship, caseName, 0);
	CheckThrottle(*ship, caseName, 0, Input(scenario, 1));
	vector<Visual> visuals;
	list<shared_ptr<Flotsam>> flotsam;
	Row(out, *ship, caseName, 0, Input(scenario, 1));
	for(int tick = 1; tick <= TICKS; ++tick)
	{
		const Controls input = Input(scenario, tick);
		CheckThrottle(*ship, caseName, tick, input);
		Command command;
		if(input.forward) command.Set(Command::FORWARD);
		if(input.back) command.Set(Command::BACK);
		if(input.stop) command.Set(Command::STOP);
		command.SetTurn(input.turn);
		ship->SetCommands(command);
		const Point before = ship->Position();
		ship->Move(visuals, flotsam);
		Check(*ship, caseName, tick);
		if(tick == 1 && scenario == "stop"
			&& (fabs(ship->Velocity().X() - velocity.X()) > 1.e-12 || fabs(ship->Velocity().Y()) > 1.e-12))
			throw runtime_error(caseName + " did not take STOP normal-velocity clamp");
		if(tick == 1 && (scenario == "forward" || scenario == "turn"))
			if(ship->Position() == before)
				throw runtime_error(caseName + " did not move on first tick");
		Row(out, *ship, caseName, tick, input);
		visuals.clear();
		flotsam.clear();
	}
	if(ship->Position() == pos)
		throw runtime_error(caseName + " has no displacement");
}
}

int main(int argc, char **argv)
{
	try
	{
		if(argc < 2)
			throw runtime_error("Expected CSV output path as first argument");
		Files::Init(const_cast<const char *const *>(argv));
		Preferences::Load();
		PluginManager::LoadSettings();
		TaskQueue::SetWorkerThreadCount(1);
		TaskQueue queue;
		PlayerInfo player;
		GameData::BeginLoad(queue, player, true, false, true).get();
		const System *system = GameData::Systems().Get("Sol");
		if(!system || !system->IsValid())
			throw runtime_error("Sol is unavailable");
		ofstream out(argv[1]);
		if(!out) throw runtime_error("Cannot open CSV output");
		out << setprecision(17);
		out << "case,tick,cmd_forward,cmd_back,cmd_stop,cmd_turn,accel,reverse_accel,turn_rate,drag,mult,x,y,vx,vy,angle_steps\n";
		for(const string modelName : {"Sparrow", "Star Barge"})
		{
			const Ship *model = GameData::Ships().Get(modelName);
			if(!model || !model->IsValid()) throw runtime_error(modelName + " model is invalid");
			for(const string scenario : {"forward", "coast", "turn", "back", "stop", "mixed"})
				Run(out, *model, system, (modelName == "Sparrow" ? "sparrow_" : "star_barge_") + scenario,
					scenario, nullptr);
		}
		Run(out, *GameData::Ships().Get("Sparrow"), system, "sparrow_pilot_route", "pilot", nullptr);
		const Outfit *reverse = GameData::Outfits().Get("X1100 Ion Reverse Thruster");
		if(!reverse || !reverse->IsDefined()) throw runtime_error("Reverse outfit missing");
		Run(out, *GameData::Ships().Get("Sparrow"), system, "sparrow_reverse_fixture", "back", reverse);
		if(!out.good()) throw runtime_error("CSV write failed");
		if(argc >= 3)
		{
			ofstream angles(argv[2], ios::binary);
			if(!angles) throw runtime_error("Cannot open angle table");
			for(int i = 0; i < 65536; ++i)
			{
				const Point unit = Angle(i * 360. / 65536.).Unit();
				const double pair[2] = {unit.X(), unit.Y()};
				angles.write(reinterpret_cast<const char *>(pair), sizeof(pair));
			}
			if(!angles.good()) throw runtime_error("Angle table write failed");
		}
		cout << setprecision(17) << "cases=14 ticks=" << TICKS << " rows=" << 14 * (TICKS + 1)
			<< " min_energy_margin=" << minimumEnergyMargin << " min_heat_margin=" << minimumHeatMargin << '\n';
		return 0;
	}
	catch(const exception &error)
	{
		cerr << "Motion oracle failed: " << error.what() << '\n';
		return 1;
	}
}
