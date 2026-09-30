/* Linked with unchanged Endless Sky objects. GPL-3.0-or-later. */
#include <bits/stdc++.h>
#define private public
#define protected public
#include "AI.h"
#include "Ship.h"
#undef private
#undef protected
#include "Files.h"
#include "GameData.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Politics.h"
#include "Preferences.h"
#include "TaskQueue.h"
#include "System.h"
using namespace std;
static string JsonString(const string &value)
{
 string out="\"";
 const char *hex="0123456789abcdef";
 for(unsigned char c:value) {
  switch(c) {
   case '"': out+="\\\""; break;
   case '\\': out+="\\\\"; break;
   case '\b': out+="\\b"; break;
   case '\f': out+="\\f"; break;
   case '\n': out+="\\n"; break;
   case '\r': out+="\\r"; break;
   case '\t': out+="\\t"; break;
   default:
    if(c<32) { out+="\\u00"; out+=hex[c>>4]; out+=hex[c&15]; }
    else out+=char(c);
  }
 }
 return out+'"';
}
struct Fixture { const char *name; double x,y,vx,vy,angle,tx,ty,radius; };
static const Fixture CASES[]={
 {"far_ahead",0,0,0,0,0,0,-900,80}, {"near_ahead",0,0,0,0,0,0,-100,80},
 {"inside_still",0,0,0,0,0,0,0,80}, {"inside_slow",0,0,.5,0,0,0,0,80},
 {"radius_equal",0,0,0,0,0,0,-80,80}, {"speed_equal",0,0,1,0,0,0,0,80},
 {"inside_fast",0,0,3,0,0,0,0,80}, {"behind",0,0,0,0,0,0,800,80},
 {"cross_right",0,0,3,0,0,900,-100,80}, {"cross_left",0,0,-3,0,0,-900,-100,80},
 {"off_axis",125,-73,2,-1,45,700,-800,80}, {"wrap",125,-73,0,0,359,700,-800,80},
 {"opposite",0,0,0,-3,180,0,-1000,80}, {"zero_dp_moving",4,5,2,-1,29,4,5,80},
 {"precision",0,0,0,0,0,3,-1000,80}, {"fast_away",0,0,0,-8,0,0,300,80},
 {"offaxis_close",0,0,0,2,35,80,-100,80}, {"small_radius",0,0,0,0,0,0,-20,15},
 {"boundary_inside",0,0,.999999,0,0,0,-79.999999,80},
 {"boundary_outside",0,0,1.000001,0,0,0,-80.000001,80}
};
int main(int argc,char **argv)
{
 try {
  if(argc<4) throw runtime_error("expected trace profile planets paths");
  Files::Init(const_cast<const char *const *>(argv));
  Preferences::Load(); PluginManager::LoadSettings(); TaskQueue::SetWorkerThreadCount(1);
  TaskQueue queue; PlayerInfo player;
  GameData::BeginLoad(queue,player,true,false,true).get();
  GameData::GetPolitics().Reset();
  const Ship *model=GameData::Ships().Get("Sparrow");
  if(!model || !model->IsValid()) throw runtime_error("Sparrow missing");
  Ship stock=*model;
  if(stock.ReverseThrust() || stock.ReverseAcceleration() || stock.ShouldUseAfterburner()
      || stock.Attributes().Get("acceleration multiplier") || stock.Crew()<stock.RequiredCrew())
   throw runtime_error("stock Sparrow unsupported");
  const double values[5]={stock.Acceleration(),stock.ReverseAcceleration(),stock.TurnRate(),
                          stock.DragForce(),1.+stock.Attributes().Get("acceleration multiplier")};
  if(values[0]<=0. || values[2]<=0. || values[3]<=0.
      || abs(stock.MaxVelocity()-values[0]/values[3])>1e-12
      || abs(stock.CrewAcceleration()-values[0])>1e-12
      || abs(stock.CrewTurnRate()-values[2])>1e-12)
   throw runtime_error("Sparrow motion getter mismatch");
  ofstream profile(argv[2],ios::binary); if(!profile) throw runtime_error("profile open");
  profile.write("ESPURS1\0",8);
  for(double d:values) profile.write(reinterpret_cast<const char *>(&d),8);
  ofstream out(argv[1]); if(!out) throw runtime_error("trace open");
  out<<setprecision(17)<<"name,x,y,vx,vy,angle,tx,ty,radius,turn,forward,back,stop,arrived\n";
  for(const auto &f:CASES) {
   Ship ship=stock;
   ship.Place(Point(f.x,f.y),Point(f.vx,f.vy),Angle(f.angle),true);
   Command c; bool arrived=AI::MoveTo(ship,c,Point(f.tx,f.ty),Point(),f.radius,1.);
   out<<f.name<<','<<f.x<<','<<f.y<<','<<f.vx<<','<<f.vy<<','<<f.angle<<','
      <<f.tx<<','<<f.ty<<','<<f.radius<<','<<c.Turn()<<','
      <<c.Has(Command::FORWARD)<<','<<c.Has(Command::BACK)<<','<<c.Has(Command::STOP)<<','<<arrived<<'\n';
  }
  /* Initial loaded politics snapshot; this is not a permanent authorization. */
  stock.SetIsYours(true);
  stock.SetSystem(GameData::Systems().Get("Sol"));
  ofstream planets(argv[3]); if(!planets) throw runtime_error("planets open");
  planets<<"{\"schema\":1,\"politics\":\"initial government reputations via Politics::Reset\","
         <<"\"ship\":\"Sparrow\",\"idle_heat\":"<<setprecision(17)
         <<stock.IdleHeat()<<",\"landing_speed\":"
         <<(stock.cache.landingSpeed>0 ? stock.cache.landingSpeed : .02f)
         <<",\"planets\":[";
  bool first=true;
  for(const char *name:{"Earth","Luna"}) {
   const Planet *p=GameData::Planets().Get(name);
   if(!p || !p->IsValid()) throw runtime_error(string("invalid planet ")+name);
   if(!first) planets<<',';
   first=false;
   planets<<"{\"name\":\""<<name<<"\",\"can_land\":"<<(p->CanLand(stock)?"true":"false")
          <<",\"description\":"<<JsonString(p->Description().ToString())
          <<",\"is_accessible\":"<<(p->IsAccessible(&stock)?"true":"false")
          <<",\"can_use_services\":"<<(p->CanUseServices()?"true":"false")
          <<",\"has_services\":"<<(p->HasServices()?"true":"false")
          <<",\"has_shipyard\":"<<(p->HasShipyard()?"true":"false")
          <<",\"has_outfitter\":"<<(p->HasOutfitter()?"true":"false")
          <<",\"can_recharge_shields\":"<<(p->GetPort().CanRecharge(Port::RechargeType::Shields,true)?"true":"false")
          <<",\"can_recharge_hull\":"<<(p->GetPort().CanRecharge(Port::RechargeType::Hull,true)?"true":"false")
          <<",\"can_recharge_energy\":"<<(p->GetPort().CanRecharge(Port::RechargeType::Energy,true)?"true":"false")
          <<",\"can_recharge_fuel\":"<<(p->GetPort().CanRecharge(Port::RechargeType::Fuel,true)?"true":"false")<<'}';
  }
  planets<<"]}\n";
  cout<<"cases="<<size(CASES)<<" accel="<<setprecision(17)<<values[0]<<" reverse="<<values[1]
      <<" turn="<<values[2]<<" drag="<<values[3]<<" mult="<<values[4]
      <<" max_velocity="<<stock.MaxVelocity()<<" landing_speed="<<stock.cache.landingSpeed<<'\n';
 } catch(const exception &e) { cerr<<e.what()<<'\n'; return 1; }
}
