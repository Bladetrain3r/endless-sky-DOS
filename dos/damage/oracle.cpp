/* Public native DamageProfile + Entity::TakeDamage oracle.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "DamageProfile.h"
#include "Entity.h"
#include "Files.h"
#include "GameData.h"
#include "Outfit.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "TaskQueue.h"
#include "Visual.h"
#include "Weapon.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

namespace {
class PlainTarget : public Entity {
public:
	PlainTarget(double shields, double hull)
	{
		levels.shields = capacities.shields = shields;
		levels.hull = capacities.hull = hull > 0. ? hull : 26.;
		levels.hull = hull;
		damageProtection.shields = 1.;
		damageProtection.hull = 1.;
		damageProtection.energy = 1.;
		damageProtection.heat = 1.;
		damageProtection.fuel = 1.;
		damageProtection.discharge = 1.;
		damageProtection.corrosion = 1.;
		damageProtection.ionization = 1.;
		damageProtection.burning = 1.;
		damageProtection.leakage = 1.;
		damageProtection.slowness = 1.;
		damageProtection.scrambling = 1.;
		damageProtection.disruption = 1.;
	}
	double Mass() const override { return 1.; }
	double MaxHeat() const override { return 1.; }
private:
	int DoTakeDamage(const DamageDealt &, const Government *, bool, bool) override { return 0; }
};

struct Case { const char *name; double shields, hull; int hits; };
constexpr array<Case, 8> CASES {{
	{"full_shields", 30., 26., 1},
	{"partial_overflow", 5.3, 26., 1},
	{"exact_depletion", 10.6, 26., 1},
	{"bare_hull", 0., 26., 1},
	{"exact_hull_zero", 0., 6.6, 1},
	{"already_zero_hull", 0., 0., 1},
	{"repeated_until_destroyed", 30., 26., 8},
	{"low_shield_repeated", 1., 13.2, 4}
}};
}

int main(int argc, char **argv)
{
	try
	{
		if(argc < 2) throw runtime_error("Expected CSV output path");
		Files::Init(const_cast<const char *const *>(argv));
		Preferences::Load();
		PluginManager::LoadSettings();
		TaskQueue::SetWorkerThreadCount(1);
		TaskQueue queue;
		PlayerInfo player;
		GameData::BeginLoad(queue, player, true, false, true).get();
		const Outfit *outfit = GameData::Outfits().Get("Energy Blaster");
		if(!outfit || !outfit->IsDefined() || !outfit->GetWeapon())
			throw runtime_error("Energy Blaster unavailable");
		const Weapon &weapon = *outfit->GetWeapon();
		if(abs(weapon.ShieldDamage() - 10.6) > 1e-12 || abs(weapon.HullDamage() - 6.6) > 1e-12
			|| weapon.DisabledDamage() != weapon.HullDamage()
			|| weapon.Piercing() || weapon.RelativeShieldDamage()
			|| weapon.RelativeHullDamage() || weapon.RelativeDisabledDamage()
			|| weapon.BlastRadius() || weapon.HasDamageDropoff())
			throw runtime_error("Energy Blaster exceeds ordinary damage slice");
		ofstream out(argv[1]);
		if(!out) throw runtime_error("Cannot open CSV");
		out << setprecision(17);
		out << "case,hit,before_shields,before_hull,dealt_shields,dealt_hull,after_shields,after_hull,destroyed\n";
		int rows = 0;
		for(const auto &test : CASES)
		{
			double shields = test.shields;
			double hull = test.hull;
			if(string(test.name) == "exact_depletion") shields = weapon.ShieldDamage();
			if(string(test.name) == "exact_hull_zero") hull = weapon.HullDamage();
			PlainTarget target(shields, hull);
			for(int hit = 1; hit <= test.hits; ++hit)
			{
				double beforeShields = target.ShieldLevel();
				double beforeHull = target.HullLevel();
				Projectile::ImpactInfo impact(weapon, Point(), 0.);
				DamageDealt dealt = DamageProfile(target, impact, true).CalculateDamage();
				vector<Visual> visuals;
				target.TakeDamage(visuals, dealt, nullptr);
				out << test.name << ',' << hit << ',' << beforeShields << ',' << beforeHull << ','
					<< dealt.Levels().shields << ',' << dealt.Levels().hull << ','
					<< target.ShieldLevel() << ',' << target.HullLevel() << ','
					<< target.IsDestroyed() << '\n';
				++rows;
			}
		}
		if(!out.good()) throw runtime_error("CSV write failed");
		cout << setprecision(17) << "cases=" << CASES.size() << " rows=" << rows
			<< " weapon=Energy_Blaster shield_damage=" << weapon.ShieldDamage()
			<< " hull_damage=" << weapon.HullDamage()
			<< " disabled_damage=" << weapon.DisabledDamage() << '\n';
		return 0;
	}
	catch(const exception &error)
	{
		cerr << "Damage oracle failed: " << error.what() << '\n';
		return 1;
	}
}
