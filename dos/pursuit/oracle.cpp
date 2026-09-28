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
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "TaskQueue.h"
using namespace std;

struct Fixture { const char *name; double sx, sy, vx, vy, angle, tx, ty, tvx, tvy; };
static const Fixture CASES[] = {
    {"ahead_far",0,0,0,0,0,0,-900,0,0},
    {"ahead_near",0,0,0,0,0,0,-100,0,0},
    {"ahead_moving_away",0,0,0,2,0,0,-100,0,0},
    {"behind",0,0,0,0,0,0,800,0,0},
    {"cross_right",0,0,3,0,0,1000,0,-2,1},
    {"cross_left",0,0,-3,0,0,-1000,0,2,-1},
    {"angle_45",125,-73,0,0,45,700,-800,2,1},
    {"angle_359",125,-73,1,-2,359,700,-800,2,1},
    {"angle_180",0,0,0,-3,180,0,-1000,0,0},
    {"zero_displacement",4,5,2,-1,29,4,5,0,0},
    {"near_precision",0,0,0,0,0,3,-1000,0,0},
    {"near_step",0,0,0,0,0,20,-1000,0,0},
    {"away_fast",0,0,0,-8,0,0,-300,0,0},
    {"off_axis_close",0,0,0,2,35,80,-100,0,0},
    {"threshold_200",0,0,0,0,0,0,-200,0,0},
    {"threshold_201",0,0,0,0,0,0,-201,0,0},
};
struct Intercept { const char *name; double x,y,vx,vy,speed; };
static const Intercept INTERCEPTS[] = {
    {"stationary",100,0,0,0,10}, {"crossing",100,50,-1,2,8},
    {"retreating",100,0,2,0,5}, {"approaching",100,0,-2,0,5},
    {"unreachable",100,0,10,0,5}, {"near_equal",100,0,4.999999,0,5},
    {"equal",100,0,5,0,5}, {"equal_approaching",100,0,-5,0,5},
    {"zero_p",0,0,1,0,5},
    {"zero_speed",100,0,-1,0,0}, {"discriminant_negative",100,100,8,0,3}
};

int main(int argc, char **argv)
{
    try {
        if(argc < 4) throw runtime_error("Expected command CSV, intercept CSV, profile");
        Files::Init(const_cast<const char *const *>(argv));
        Preferences::Load(); PluginManager::LoadSettings();
        TaskQueue::SetWorkerThreadCount(1);
        TaskQueue queue; PlayerInfo player;
        GameData::BeginLoad(queue, player, true, false, true).get();
        const Ship *model = GameData::Ships().Get("Star Barge");
        if(!model || !model->IsValid()) throw runtime_error("Star Barge missing");
        Ship stock = *model;
        if(stock.ReverseThrust() || stock.ShouldUseAfterburner()
           || stock.ReverseAcceleration() || stock.Attributes().Get("acceleration multiplier")
           || stock.Weapons().size() != 1 || !stock.Weapons().front().IsTurret())
            throw runtime_error("Stock Star Barge fitting changed");
        const double values[5] = {stock.Acceleration(), stock.ReverseAcceleration(),
            stock.TurnRate(), stock.DragForce(), 1. + stock.Attributes().Get("acceleration multiplier")};
        if(values[0] <= 0. || values[2] <= 0. || values[3] < 0.)
            throw runtime_error("Invalid motion parameters");
        ofstream profile(argv[3], ios::binary);
        if(!profile) throw runtime_error("Profile open failed");
        profile.write("ESPURS1\0", 8);
        for(double value : values) profile.write(reinterpret_cast<const char *>(&value), 8);
        ofstream out(argv[1]); if(!out) throw runtime_error("Command CSV open failed");
        out << setprecision(17) << "name,sx,sy,vx,vy,angle,tx,ty,tvx,tvy,turn,forward,back,afterburner\n";
        for(const auto &f : CASES) {
            Ship ship = stock; Ship target = stock;
            ship.Place(Point(f.sx, f.sy), Point(f.vx, f.vy), Angle(f.angle), true);
            target.Place(Point(f.tx, f.ty), Point(f.tvx, f.tvy), Angle(0.), true);
            Command command; FireCommand firing;
            AI::MoveToAttack(ship, command, target, firing);
            Point direction = target.Position() - ship.Position();
            if(command.Turn() != AI::TurnToward(ship, direction))
                throw runtime_error(string(f.name) + " TargetAim fallback mismatch");
            out << f.name << ',' << f.sx << ',' << f.sy << ',' << f.vx << ',' << f.vy << ','
                << f.angle << ',' << f.tx << ',' << f.ty << ',' << f.tvx << ',' << f.tvy << ','
                << command.Turn() << ',' << command.Has(Command::FORWARD) << ','
                << command.Has(Command::BACK) << ',' << command.Has(Command::AFTERBURNER) << '\n';
        }
        ofstream intercept(argv[2]); if(!intercept) throw runtime_error("Intercept CSV open failed");
        intercept << setprecision(17) << "name,x,y,vx,vy,speed,time\n";
        for(const auto &f : INTERCEPTS)
            intercept << f.name << ',' << f.x << ',' << f.y << ',' << f.vx << ',' << f.vy
                << ',' << f.speed << ',' << AI::RendezvousTime(Point(f.x,f.y), Point(f.vx,f.vy),f.speed) << '\n';
        if(!out.good() || !intercept.good() || !profile.good()) throw runtime_error("Output write failed");
        cout << setprecision(17) << "commands=" << size(CASES) << " intercepts=" << size(INTERCEPTS)
            << " accel=" << values[0] << " reverse=" << values[1] << " turn=" << values[2]
            << " drag=" << values[3] << " mult=" << values[4] << '\n';
    } catch(const exception &e) { cerr << "Pursuit oracle failed: " << e.what() << '\n'; return 1; }
}
