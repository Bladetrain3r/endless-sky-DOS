/* SPDX-License-Identifier: GPL-3.0-or-later
 * Isolated fixture linked to unchanged upstream objects. Never opens user saves.
 */
#include <bits/stdc++.h>
#define private public
#include "PlayerInfo.h"
#include "TradingPanel.h"
#undef private
#include "Account.h"
#include "CargoHold.h"
#include "DataFile.h"
#include "DataWriter.h"
#include "Files.h"
#include "GameData.h"
#include "PilotProfile.h"
#include "Planet.h"
#include "Outfit.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "Random.h"
#include "Ship.h"
#include "StartConditions.h"
#include "System.h"
#include "TaskQueue.h"
#include "UI.h"
using namespace std;
static const string COMMODITY="Food";
static const string UUID="10000000-0000-4000-8000-000000000001";
static void require(bool condition,const string &message)
{ if(!condition) throw runtime_error(message); }
static int pending(const PlayerInfo &p)
{
 DataWriter writer;GameData::WriteEconomy(writer);istringstream input(writer.SaveToString());DataFile file(input);
 for(const auto &node:file) if(node.Token(0)=="economy")
  for(const auto &child:node) if(child.Token(0)=="purchases")
   for(const auto &purchase:child)
    if(purchase.Size()>=3 && purchase.Token(0)==p.GetSystem()->TrueName() && purchase.Token(1)==COMMODITY)
     return purchase.Value(2);
 return 0;
}
struct State {
 int64_t credits,basis;int qty,capacity,free,date,price,purchases;
 string uuid,system,planet;double shields,hull,energy,heat;
 bool operator==(const State &) const=default;
};
static State state(PlayerInfo &p)
{
 const Ship *ship=p.Flagship();require(ship,"missing flagship");
 return {p.Accounts().Credits(),p.GetBasis(COMMODITY,p.Cargo().Get(COMMODITY)),
  p.Cargo().Get(COMMODITY),p.Cargo().Size(),p.Cargo().Free(),p.GetDate().DaysSinceEpoch(),
  p.GetSystem()->Trade(COMMODITY),pending(p),ship->UUID().ToString(),
  p.GetSystem()->TrueName(),p.GetPlanet()->TrueName(),ship->ShieldLevel(),ship->HullLevel(),
  ship->EnergyLevel(),ship->HeatLevel()};
}
static void emit(ostream &out,const string &phase,const State &s)
{
 out<<phase<<','<<s.credits<<','<<s.basis<<','<<s.qty<<','<<s.capacity<<','<<s.free<<','
    <<s.date<<','<<s.price<<','<<s.purchases<<','<<s.uuid<<','<<s.system<<','<<s.planet<<','
    <<setprecision(17)<<s.shields<<','<<s.hull<<','<<s.energy<<','<<s.heat<<'\n';
}
static void buy(PlayerInfo &p,int amount) { TradingPanel panel(p);panel.Buy(amount); }
int main(int argc,char **argv)
{
 try {
  require(argc>=2,"expected output directory");filesystem::path work=argv[1];
  Files::Init(const_cast<const char *const *>(argv));Preferences::Load();PluginManager::LoadSettings();
  TaskQueue::SetWorkerThreadCount(1);TaskQueue queue;PlayerInfo player;
  GameData::BeginLoad(queue,player,true,false,true).get();
  // PlayerInfo::Clear reverts these snapshots; initialize them before New/Load.
  GameData::FinishLoading();
  auto profile=PilotProfile::NewProfile();profile->New(GameData::DefaultGamerules());
  require(!GameData::StartOptions().empty(),"missing starts");
  player.New(GameData::StartOptions().front(),profile);player.SetName("ESDOS","Fixture");
  Random::Seed(9302026);player.SetSystem(*GameData::Systems().Get("Sol"));
  player.SetPlanet(GameData::Planets().Get("Earth"));
  require(player.GiftShip(GameData::Ships().Get("Sparrow"),"Fixture Sparrow","")!=nullptr,"gift failed");
  player.Flagship()->SetUUID(EsUuid::FromString(UUID));
  player.Accounts().AddCredits(10000-player.Accounts().Credits());
  UI ui;player.freshlyLoaded=true;player.Land(ui);
  int selected=-1;
  for(size_t i=0;i<GameData::Commodities().size();++i)
   if(GameData::Commodities()[i].name==COMMODITY) selected=i;
  require(selected>=0,"Food missing");player.SetMapColoring(selected);
  State before=state(player);require(before.qty==0 && before.free>=4 && before.price>0,"bad initial cargo");
  require(player.CanBeSaved(),"landed save ineligible");
  // Native-owned capacity/resource/movement limits for the bounded DOS pilot.
  const Ship &stock=*player.Flagship();
  ofstream limits(work/"LIMITS.json");require(bool(limits),"limits open failed");
  limits<<setprecision(17)<<"{\"capacity\":"<<before.capacity
    <<",\"max_crew\":"<<stock.Attributes().Get("bunks")
    <<",\"required_crew\":"<<stock.RequiredCrew()<<",\"fuel\":"<<stock.MaxFuel()
    <<",\"shields\":"<<stock.MaxShields()<<",\"hull\":"<<stock.MaxHull()
    <<",\"date\":["<<player.GetDate().Day()<<','<<player.GetDate().Month()<<','<<player.GetDate().Year()
    <<"],\"cargo_profiles\":[";
  for(int tons=0;tons<=before.capacity;++tons) {
   Ship loaded(stock);loaded.Cargo().Add(COMMODITY,tons);
   if(tons) limits<<',';
   limits<<'['<<loaded.Acceleration()<<','<<loaded.ReverseAcceleration()<<','<<loaded.TurnRate()
     <<','<<loaded.DragForce()<<",1,"<<loaded.MaxHeat()<<','<<loaded.IdleHeat()<<']';
  }
  limits<<"]}\n";limits.close();require(bool(limits),"limits write failed");
  auto save=[&](const char *name){player.Save((work/name).string());profile->Save();};
  save("BEFORE.TXT");
  buy(player,3);State after=state(player);
  require(after.qty==3 && after.credits==before.credits-3*before.price && after.basis==3*before.price,
   "purchase arithmetic mismatch");require(after.purchases==before.purchases,"buy unexpectedly changes pending sales");
  save("AFTER.TXT");
  // A transaction isolates on-disk saves; it does not roll back live state.
  player.StartTransaction();buy(player,1);State next=state(player);save("TXN.TXT");
  player.FinishTransaction();save("COMMIT.TXT");
  auto reload=[&](const char *name){player.Load(work/name,profile);player.Land(ui);player.SetMapColoring(selected);};
  reload("TXN.TXT");require(state(player)==after,"transaction save did not retain pre-transaction state");
  reload("COMMIT.TXT");require(state(player)==next,"committed transaction reload mismatch");
  reload("AFTER.TXT");State loaded=state(player);require(loaded==after,"normalized reload mismatch");
  buy(player,1);require(state(player)==next,"next purchase differs after reload");
  // The native UI clamps quantities, rather than rejecting an oversized request.
  reload("AFTER.TXT");State clampBefore=state(player);buy(player,1000000);State clamped=state(player);
  int amount=min<int64_t>(clampBefore.free,clampBefore.credits/clampBefore.price);
  require(clamped.qty==clampBefore.qty+amount && clamped.credits==clampBefore.credits-int64_t(amount)*clampBefore.price,
   "quantity clamp mismatch");
  buy(player,-1000000);State sold=state(player);require(sold.qty==0 && sold.basis==0,"oversell clamp mismatch");
  require(sold.purchases==-clamped.qty,"pending sales mismatch");
  save("SOLD.TXT");reload("SOLD.TXT");require(state(player)==sold,"pending economy did not survive reload");
  player.SetPlanet(nullptr);require(!player.CanBeSaved(),"in-flight save accepted");
  player.SetPlanet(GameData::Planets().Get("Earth"));profile->SetLock();
  require(!player.CanBeSaved(),"locked pilot save accepted");profile->SetLock(false);
  ofstream trace(work/"trace.csv");require(bool(trace),"trace open failed");
  trace<<"phase,credits,basis,qty,capacity,free,date_days,unit_price,pending_sales,ship_uuid,system,planet,shields,hull,energy,heat\n";
  emit(trace,"before",before);emit(trace,"after_purchase",after);emit(trace,"after_reload",loaded);
  emit(trace,"next_purchase",next);emit(trace,"clamped_purchase",clamped);emit(trace,"clamped_sale",sold);
  cout<<"purchase_reload=pass next_action=pass transaction_isolation=pass quantity_clamps=pass "
      <<"pending_sale_reload=pass save_eligibility=pass\n";
 } catch(const exception &e) {cerr<<e.what()<<'\n';return 1;}
}
