/* Native resolved-world snapshot exporter.
Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later

Linked against an unchanged Endless Sky native object build. A staged System.h
adds a friend declaration for ExportTrade(), solely to expose actual map entries.
*/

#include "System.h"
#include "Files.h"
#include "GameData.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "TaskQueue.h"
#include "image/Sprite.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

// This function is granted access by the staged, build-only System.h. Iterating
// the map is essential: System::Trade(name) returns zero for absent entries.
map<string, int> ExportTrade(const System &system)
{
	map<string, int> prices;
	for(const auto &[name, price] : system.trade)
		prices.emplace(name, price.price);
	return prices;
}

static void String(ostream &out, const string &value)
{
	static constexpr char HEX[] = "0123456789abcdef";
	out << '"';
	for(unsigned char ch : value)
	{
		switch(ch)
		{
			case '"': out << "\\\""; break;
			case '\\': out << "\\\\"; break;
			case '\b': out << "\\b"; break;
			case '\f': out << "\\f"; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default:
				if(ch < 0x20)
					out << "\\u00" << HEX[ch >> 4] << HEX[ch & 15];
				else
					out << ch;
		}
	}
	out << '"';
}

static void Number(ostream &out, double value)
{
	if(!isfinite(value))
		throw runtime_error("Non-finite world coordinate or orbit value");
	if(value == 0. && signbit(value))
	{
		out << "-0.0";
		return;
	}
	out << setprecision(numeric_limits<double>::max_digits10) << value;
}

template<class Items> static void Strings(ostream &out, const Items &items)
{
	out << '[';
	bool first = true;
	for(const auto &item : items)
	{
		if(!first)
			out << ',';
		first = false;
		String(out, item);
	}
	out << ']';
}

template<class Type> static map<const Type *, string> Keys(const Set<Type> &items)
{
	map<const Type *, string> keys;
	for(const auto &[key, value] : items)
		keys.emplace(&value, key);
	return keys;
}

template<class Type> static const string &Key(const map<const Type *, string> &keys, const Type *pointer)
{
	auto it = keys.find(pointer);
	if(it == keys.end())
		throw runtime_error("World reference missing from native registry");
	return it->second;
}

static void Export(ostream &out)
{
	const auto &systems = GameData::Systems();
	const auto &planets = GameData::Planets();
	const auto systemKeys = Keys(systems);
	const auto planetKeys = Keys(planets);
	out << "{\"schema\":1,\"systems\":[";
	bool firstSystem = true;
	for(const auto &[name, system] : systems)
	{
		if(!firstSystem)
			out << ',';
		firstSystem = false;
		out << "{\"name\":"; String(out, name);
		out << ",\"display\":"; String(out, system.DisplayName());
		out << ",\"valid\":" << (system.IsValid() ? "true" : "false");
		out << ",\"hidden\":" << (system.Hidden() ? "true" : "false");
		out << ",\"shrouded\":" << (system.Shrouded() ? "true" : "false");
		out << ",\"inaccessible\":" << (system.Inaccessible() ? "true" : "false");
		out << ",\"x\":"; Number(out, system.Position().X());
		out << ",\"y\":"; Number(out, system.Position().Y());
		out << ",\"jump_range\":"; Number(out, system.JumpRange());
		out << ",\"arrival_hyper\":"; Number(out, system.ExtraHyperArrivalDistance());
		out << ",\"arrival_jump\":"; Number(out, system.ExtraJumpArrivalDistance());
		out << ",\"departure_hyper\":"; Number(out, system.HyperDepartureDistance());
		out << ",\"departure_jump\":"; Number(out, system.JumpDepartureDistance());
		out << ",\"habitable\":"; Number(out, system.HabitableZone());
		out << ",\"attributes\":"; Strings(out, system.Attributes());
		vector<string> links;
		for(const System *link : system.Links())
			links.push_back(Key(systemKeys, link));
		sort(links.begin(), links.end());
		out << ",\"links\":"; Strings(out, links);
		out << ",\"objects\":[";
		bool firstObject = true;
		for(const StellarObject &object : system.Objects())
		{
			if(!firstObject)
				out << ',';
			firstObject = false;
			out << "{\"index\":" << object.Index() << ",\"parent\":" << object.Parent();
			out << ",\"planet\":";
			if(object.GetPlanet()) String(out, Key(planetKeys, object.GetPlanet())); else out << "null";
			out << ",\"sprite\":";
			if(object.GetSprite()) String(out, object.GetSprite()->Name()); else out << "null";
			out << ",\"distance\":"; Number(out, object.Distance());
			out << ",\"period\":"; Number(out, object.Period());
			out << ",\"offset\":"; Number(out, object.Offset());
			out << '}';
		}
		out << "],\"trade\":[";
		bool firstPrice = true;
		for(const auto &[commodity, price] : ExportTrade(system))
		{
			if(!firstPrice)
				out << ',';
			firstPrice = false;
			out << "{\"commodity\":"; String(out, commodity);
			out << ",\"price\":" << price << '}';
		}
		out << "]}";
	}
	out << "],\"planets\":[";
	bool firstPlanet = true;
	for(const auto &[name, planet] : planets)
	{
		if(!firstPlanet)
			out << ',';
		firstPlanet = false;
		out << "{\"name\":"; String(out, name);
		out << ",\"display\":"; String(out, planet.DisplayName());
		out << ",\"valid\":" << (planet.IsValid() ? "true" : "false");
		out << ",\"inhabited\":" << (planet.IsInhabited() ? "true" : "false");
		out << ",\"attributes\":"; Strings(out, planet.Attributes());
		vector<string> memberships;
		for(const System *system : planet.Systems())
			memberships.push_back(Key(systemKeys, system));
		sort(memberships.begin(), memberships.end());
		out << ",\"systems\":"; Strings(out, memberships);
		out << '}';
	}
	out << "],\"commodities\":[";
	const auto &commodities = GameData::Commodities();
	for(size_t i = 0; i < commodities.size(); ++i)
	{
		if(i) out << ',';
		out << "{\"name\":"; String(out, commodities[i].name);
		out << ",\"low\":" << commodities[i].low;
		out << ",\"high\":" << commodities[i].high;
		out << ",\"ordinal\":" << i << '}';
	}
	out << "]}\n";
}

int main(int argc, char **argv)
{
	try
	{
		Files::Init(const_cast<const char *const *>(argv));
		Preferences::Load();
		PluginManager::LoadSettings();
		TaskQueue::SetWorkerThreadCount(1);
		TaskQueue queue;
		PlayerInfo player;
		GameData::BeginLoad(queue, player, true, false, true).get();
		Export(cout);
		return cout.good() ? 0 : 1;
	}
	catch(const exception &error)
	{
		cerr << "World export failed: " << error.what() << '\n';
		return 1;
	}
}
