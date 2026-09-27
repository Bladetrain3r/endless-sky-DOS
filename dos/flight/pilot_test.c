/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pilot.h"
#include "input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line%d: %s\n",__LINE__,#x); return 1; } } while(0)

int main(void)
{
    Pilot p, saved;
    Scene scene={0};
    FILE *f;
    char line[1024];
    unsigned steps=0;
    CHECK(pilot_load(&p,".work/flight/run/PILOT.DAT"));
    pilot_reset(&p,&scene);
    f=fopen(".work/motion/native.csv","r");
    CHECK(f && fgets(line,sizeof(line),f));
    while(fgets(line,sizeof(line),f)) {
        char name[80];
        int tick,forward,back,stop;
        unsigned angle;
        double turn,accel,reverse,rate,drag,mult,x,y,vx,vy;
        CHECK(sscanf(line,"%79[^,],%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%u",
            name,&tick,&forward,&back,&stop,&turn,&accel,&reverse,&rate,&drag,&mult,
            &x,&y,&vx,&vy,&angle)==16);
        if(strcmp(name,"sparrow_pilot_route")) continue;
        if(tick) {
            CHECK((unsigned)tick==steps+1);
            pilot_keys(&p,&scene,pilot_replay_keys(p.ticks));
            CHECK(p.command.forward==forward && p.command.back==back && p.command.turn==turn);
            pilot_step(&p,&scene);
            ++steps;
        }
        CHECK(fabs(p.state.x-x-275.)<1e-7 && fabs(p.state.y-y-373.)<1e-7);
        CHECK(fabs(p.state.vx-vx)<1e-9 && fabs(p.state.vy-vy)<1e-9 && p.state.angle==angle);
        CHECK((int)lround(p.state.x)-scene.camera_x==400);
        CHECK((int)lround(p.state.y)-scene.camera_y==300);
    }
    CHECK(!ferror(f) && steps==900);
    fclose(f);
    pilot_keys(&p,&scene,INPUT_CAMERA);
    CHECK(!p.follow);
    {
        int x=scene.camera_x,y=scene.camera_y;
        pilot_keys(&p,&scene,INPUT_CAMERA); /* Holding does not repeatedly toggle. */
        pilot_step(&p,&scene);
        CHECK(!p.follow && scene.camera_x==x && scene.camera_y==y);
    }
    pilot_keys(&p,&scene,0);
    pilot_keys(&p,&scene,INPUT_CAMERA);
    CHECK(p.follow);
    pilot_keys(&p,&scene,INPUT_RESET);
    CHECK(p.state.x==400. && p.state.y==300. && p.state.vx==0. && p.state.vy==0.);
    CHECK(p.follow && !scene.camera_x && !scene.camera_y);
    pilot_keys(&p,&scene,INPUT_LEFT|INPUT_RIGHT|INPUT_FORWARD|INPUT_BACK);
    pilot_step(&p,&scene);
    CHECK(p.state.x==400. && p.state.y==300. && !p.state.angle);
    pilot_keys(&p,&scene,INPUT_FORWARD|INPUT_RIGHT);
    pilot_step(&p,&scene);
    CHECK(p.state.vx>0. && p.state.vy<0. && p.state.angle>0);
    saved=p;
    pilot_keys(&p,&scene,0);
    pilot_step(&p,&scene);
    CHECK(p.state.vx==saved.state.vx && p.state.vy==saved.state.vy);
    saved=p;
    CHECK(!pilot_load(&p,".work/flight/bad-pilot.dat"));
    CHECK(!memcmp(&p,&saved,sizeof(p)));
    puts("status=pass\nnative_route_ticks=900\ncontrols_and_camera=pass\ninvalid_profile=reject");
    return 0;
}
