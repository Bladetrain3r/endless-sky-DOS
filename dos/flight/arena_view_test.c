/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arena_view.h"
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
    Sprite sprites[6]={{0}};static Practice p;Arena a;Opponent o;Threat t;Pilot pilot={0};Scene scene={0};
    unsigned i,j,cases=0;
    unsigned char *frame=malloc(VIDEO_PIXELS),*copy=malloc(VIDEO_PIXELS),*blend=malloc(1048576),*add=malloc(65536);
    CHECK(frame && copy && blend && add);
    read_bytes("BLEND.LUT",blend,1048576);read_bytes("ADD.LUT",add,65536);
    for(i=0;i<6;++i) CHECK(sprite_load(&sprites[i],names[i]));
    CHECK(practice_load(&p,"BLASTER.DAT","MASKS.BIN"));
    CHECK(practice_damage_load(&p,"DAMAGE.DAT") && practice_shields_load(&p,"SHIELD.DAT"));
    CHECK(practice_resources_load(&p,"RESOURCE.DAT",0));
    CHECK(opponent_load(&o,"PURSUIT.DAT") && threat_load(&t,"PLAYER.DAT"));
    CHECK(arena_init(&a,&p,&o,0,0));pilot.state=(MotionState){400,300,0,0,0};
    for(i=0;i<sizeof(points)/sizeof(points[0]);++i) for(j=0;j<8;++j) {
        Arena before;Practice player_before;Threat threat_before;Pilot pilot_before;
        a.enemies[0].motion.x=points[i][0];a.enemies[0].motion.y=points[i][1];
        a.enemies[1].motion.x=800.-points[i][0];a.enemies[1].motion.y=600.-points[i][1];
        a.enemies[0].motion.angle=(uint16_t)(j*8192);
        a.enemies[0].explosion=48-j*6;
        a.enemies[0].bolts[0]=(Bolt){points[i][0],points[i][1],0,-13,48,1,0};
        p.bolts[0]=a.enemies[0].bolts[0];
        a.selected=(int)(j%3)-1;a.enemies[0].destroyed=j>=4;a.enemies[1].destroyed=j>=6;
        t.disabled=j==6;t.destroyed=j==7;p.destructible=j%2;
        scene.camera_x=j%2?24000:0;scene.camera_y=j%2?-24000:0;
        before=a;player_before=p;threat_before=t;pilot_before=pilot;
        arena_view_draw(frame,&a,&p,&pilot,&t,&scene,sprites,blend,add);
        memcpy(copy,frame,VIDEO_PIXELS);
        arena_view_draw(frame,&a,&p,&pilot,&t,&scene,sprites,blend,add);
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
