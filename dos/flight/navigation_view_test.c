/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "navigation_view.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while(0)
static void read_bytes(const char *name,unsigned char *out,size_t count)
{
    FILE *f=fopen(name,"rb");CHECK(f);CHECK(fread(out,1,count,f)==count);fclose(f);
}
int main(void)
{
    const char *names[]={"SPARROW.SPR","BARGE.SPR","FALCON.SPR","EARTH.SPR","LUNA.SPR","GLOW.SPR"};
    const double points[][2]={{0,0},{400,300},{800,600},{-250,300},{1050,300},
        {400,-250},{400,850},{-1e20,1e20},{1e20,-1e20},{20,30},{770,475}};
    Sprite sprites[6]={{0}};static Practice p;Navigation a;Threat t;Pilot pilot={0};Scene scene={0};
    unsigned i,j,cases=0;
    unsigned char *frame=malloc(VIDEO_PIXELS),*copy=malloc(VIDEO_PIXELS),*blend=malloc(1048576),*add=malloc(65536);
    CHECK(frame && copy && blend && add);
    read_bytes("BLEND.LUT",blend,1048576);read_bytes("ADD.LUT",add,65536);
    for(i=0;i<6;++i) CHECK(sprite_load(&sprites[i],names[i]));
    CHECK(practice_load(&p,"BLASTER.DAT","MASKS.BIN"));
    CHECK(practice_damage_load(&p,"DAMAGE.DAT") && practice_shields_load(&p,"SHIELD.DAT"));
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",0));
    CHECK(threat_load(&t,"PLAYER.DAT"));
    scene.earth_x=550;scene.earth_y=345;scene.luna_x=850;scene.luna_y=150;
    CHECK(navigation_load(&a,&scene,"NAV.DAT"));pilot.state=(MotionState){400,300,0,0,0};
    for(i=0;i<sizeof(points)/sizeof(points[0]);++i) for(j=0;j<8;++j) {
        Navigation before;Practice player_before;Threat threat_before;Pilot pilot_before;
        a.destinations[0].x=points[i][0];a.destinations[0].y=points[i][1];
        a.destinations[1].x=800.-points[i][0];a.destinations[1].y=600.-points[i][1];
        pilot.state.angle=(uint16_t)(j*8192);
        pilot.state.x=points[i][0];pilot.state.y=points[i][1];
        p.bolts[0]=(Bolt){points[i][0],points[i][1],0,-13,48,1,0};
        a.selected=(int)(j%2);a.phase=(NavPhase)(j%4);a.zoom=j/7.f;
        t.disabled=j==6;t.destroyed=j==7;p.destructible=j%2;
        scene.camera_x=j%2?24000:0;scene.camera_y=j%2?-24000:0;
        before=a;player_before=p;threat_before=t;pilot_before=pilot;
        navigation_view_draw(frame,&a,&p,&pilot,&t,&scene,sprites,blend,add);
        memcpy(copy,frame,VIDEO_PIXELS);
        navigation_view_draw(frame,&a,&p,&pilot,&t,&scene,sprites,blend,add);
        CHECK(!memcmp(frame,copy,VIDEO_PIXELS));
        CHECK(!memcmp(&a,&before,sizeof(a)) && !memcmp(&p,&player_before,sizeof(p)));
        CHECK(!memcmp(&t,&threat_before,sizeof(t)) && !memcmp(&pilot,&pilot_before,sizeof(pilot)));
        ++cases;
    }
    for(i=0;i<6;++i) sprite_free(&sprites[i]);
    free(frame);free(copy);free(blend);free(add);
    printf("status=pass\nrender_cases=%u\nrender_deterministic_and_read_only=pass\n",cases);
    return 0;
}
