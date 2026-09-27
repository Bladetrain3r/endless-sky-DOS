/* Native Projectile constructor and Move oracle.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later

Linked against unchanged upstream objects. No private access or class changes.
*/

#include "Angle.h"
#include "Files.h"
#include "GameData.h"
#include "Outfit.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "Projectile.h"
#include "Ship.h"
#include "TaskQueue.h"
#include "Visual.h"
#include "Weapon.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
struct Case {
	const char *name;
	double x, y;
	double parentVx, parentVy, parentHeading;
	double launchHeading;
};

constexpr array<Case, 12> CASES {{
	{"north_still", 0., 0., 0., 0., 0., 0.},
	{"east_still", 100., -50., 0., 0., 0., 90.},
	{"south_still", -23.5, 41.25, 0., 0., 0., 180.},
	{"west_still", 25., 75., 0., 0., 0., 270.},
	{"wrap_359", 125., -73., 0., 0., 270., 359.},
	{"wrap_near_360", -47., 9., 0., 0., 1., 359.9945},
	{"wrap_negative", -100., -200., 0., 0., 180., -0.01},
	{"diagonal_parent", 31., -83., 2., -3., 45., 45.},
	{"opposing_parent", 0., 0., -2., 3., 225., 45.},
	{"fast_parent", 425., -315., 12., 7.5, 92., 271.},
	{"fraction_parent", -.125, .375, .125, -.875, 359., 123.456},
	{"reverse_parent", 80., 25., -10.625, 0., 180., 90.}
}};

int Steps(const Angle &angle)
{
	return static_cast<int>(llround(angle.AbsDegrees() * 65536. / 360.)) & 65535;
}

void Row(ostream &out, const Case &test, const Projectile &shot, int frame,
	const vector<Visual> &visuals, const vector<Projectile> &spawned)
{
	out << test.name << ',' << frame << ',' << test.x << ',' << test.y << ','
		<< Steps(Angle(test.parentHeading)) << ',' << Steps(Angle(test.launchHeading)) << ','
		<< test.parentVx << ',' << test.parentVy << ','
		<< shot.Position().X() << ',' << shot.Position().Y() << ','
		<< shot.Velocity().X() << ',' << shot.Velocity().Y() << ','
		<< Steps(shot.Facing()) << ',' << shot.DistanceTraveled() << ','
		<< shot.IsDead() << ',' << shot.ShouldBeRemoved() << ',' << shot.Clip() << ','
		<< spawned.size() << ',' << visuals.size() << '\n';
}

void Run(ostream &out, const Ship &model, const Weapon *weapon)
{
	for(const Case &test : CASES)
	{
		Ship parent(model);
		parent.Place(Point(500., -250.), Point(test.parentVx, test.parentVy),
			Angle(test.parentHeading), true);
		Projectile shot(parent, Point(test.x, test.y), Angle(test.launchHeading), weapon);
		if(shot.IsDead() || shot.ShouldBeRemoved())
			throw runtime_error(string(test.name) + " starts dead");
		vector<Visual> visuals;
		vector<Projectile> spawned;
		Row(out, test, shot, 0, visuals, spawned);
		for(int frame = 1; frame <= 49; ++frame)
		{
			shot.Move(visuals, spawned);
			Row(out, test, shot, frame, visuals, spawned);
			if(frame == 47 && (shot.IsDead() || shot.ShouldBeRemoved()))
				throw runtime_error(string(test.name) + " ended early");
			if(frame == 48 && (!shot.IsDead() || !shot.ShouldBeRemoved()))
				throw runtime_error(string(test.name) + " did not expire at 48");
			visuals.clear();
			spawned.clear();
		}
	}
}
}

int main(int argc, char **argv)
{
	try
	{
		if(argc < 2)
			throw runtime_error("Expected CSV output path");
		Files::Init(const_cast<const char *const *>(argv));
		Preferences::Load();
		PluginManager::LoadSettings();
		TaskQueue::SetWorkerThreadCount(1);
		TaskQueue queue;
		PlayerInfo player;
		GameData::BeginLoad(queue, player, true, false, true).get();
		const Ship *model = GameData::Ships().Get("Sparrow");
		const Outfit *outfit = GameData::Outfits().Get("Energy Blaster");
		if(!model || !model->IsValid() || !outfit || !outfit->IsDefined() || !outfit->GetWeapon())
			throw runtime_error("Sparrow or Energy Blaster unavailable");
		const Weapon &weapon = *outfit->GetWeapon();
		if(weapon.Lifetime() != 48 || weapon.Velocity() != 10.625
			|| weapon.RandomLifetime() || weapon.RandomVelocity() || weapon.Acceleration()
			|| weapon.Drag() || weapon.Turn() || weapon.Homing()
			|| !weapon.LiveEffects().empty() || !weapon.DieEffects().empty()
			|| !weapon.Submunitions().empty())
			throw runtime_error("Energy Blaster differs from bounded ordinary projectile slice");
		ofstream out(argv[1]);
		if(!out) throw runtime_error("Cannot open CSV");
		out << setprecision(17);
		out << "case,frame,launch_x,launch_y,parent_angle_steps,launch_angle_steps,parent_vx,parent_vy,x,y,vx,vy,angle_steps,distance_traveled,is_dead,is_removed,clip,spawned_projectiles,visuals\n";
		Run(out, *model, &weapon);
		if(!out.good()) throw runtime_error("CSV write failed");
		cout << setprecision(17) << "cases=" << CASES.size() << " rows=" << CASES.size() * 50
			<< " weapon=Energy_Blaster lifetime=" << weapon.Lifetime()
			<< " random_lifetime=" << weapon.RandomLifetime()
			<< " velocity=" << weapon.Velocity()
			<< " random_velocity=" << weapon.RandomVelocity()
			<< " reload=" << weapon.Reload()
			<< " inaccuracy=" << weapon.Inaccuracy()
			<< " acceleration=" << weapon.Acceleration()
			<< " drag=" << weapon.Drag()
			<< " turn=" << weapon.Turn()
			<< " homing=" << weapon.Homing()
			<< " fade_out=" << weapon.FadeOut()
			<< " penetration=" << weapon.PenetrationCount()
			<< " live_effects=" << weapon.LiveEffects().size()
			<< " die_effects=" << weapon.DieEffects().size()
			<< " submunitions=" << weapon.Submunitions().size() << '\n';
		return 0;
	}
	catch(const exception &error)
	{
		cerr << "Projectile oracle failed: " << error.what() << '\n';
		return 1;
	}
}
