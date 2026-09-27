/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void reference_scene_draw(unsigned char *,const Scene *,const Sprite *,
                          const unsigned char *,const unsigned char *,SceneProfile *);
static int load(const char *name,unsigned char *data,size_t size)
{
    FILE *f=fopen(name,"rb"); int ok;
    if(!f) return 0;
    ok=fread(data,1,size,f)==size && fgetc(f)==EOF;
    fclose(f); return ok;
}
int main(void)
{
    const char *names[]={"SPARROW.SPR","BARGE.SPR","FALCON.SPR","EARTH.SPR","LUNA.SPR","GLOW.SPR"};
    WorldObject objects[2]={{.index=0,.parent=-1,.distance=100.,.sprite="planet/earth"},
                            {.index=1,.parent=0,.distance=201.,.sprite="planet/luna"}};
    ActiveSystem system={.object_count=2,.objects=objects};
    Scene moving,fixed; Sprite sprites[6]={{0}};
    unsigned char *a=malloc(480000),*b=malloc(480000),*blend=malloc(1048576),*add=malloc(65536);
    unsigned i,tick,cases=0; int last_x=0,last_y=0;
    if(!a||!b||!blend||!add||!load("BLEND.LUT",blend,1048576)||!load("ADD.LUT",add,65536)) return 1;
    for(i=0;i<6;++i) if(!sprite_load(&sprites[i],names[i])) return 2;
    if(!scene_init(&moving,&system,20)||!scene_init(&fixed,&system,20)) return 3;
    moving.camera_enabled=1;
    if(moving.luna_y!=144) return 4;
    for(tick=0;tick<=2880;++tick) {
        if(tick) { scene_step(&moving); scene_step(&fixed); }
        if(abs(moving.camera_x)>240 || abs(moving.camera_y)>90 ||
           abs(moving.camera_x-last_x)>1 || abs(moving.camera_y-last_y)>1 ||
           fixed.camera_x || fixed.camera_y ||
           memcmp(moving.ships,fixed.ships,sizeof(moving.ships))) return 5;
        last_x=moving.camera_x; last_y=moving.camera_y;
        if(tick%73==0 || tick%240==0 || tick%240==239) {
            scene_draw(a,&moving,sprites,blend,add,NULL);
            reference_scene_draw(b,&moving,sprites,blend,add,NULL);
            if(memcmp(a,b,480000)) { printf("frame_mismatch_tick=%u\n",tick); return 6; }
            ++cases;
        }
    }
    if(moving.camera_x || moving.camera_y) return 7;
    /* Free flight can move thousands of pixels beyond the old panning route.
     * At these offsets all foreground objects are outside the screen. Stars
     * must repeat exactly after800x600 pixels of parallax travel. */
    for(i=0;i<2;++i) {
        int sign=i?-1:1;
        moving.camera_x=sign*24000; moving.camera_y=sign*24000;
        scene_draw(a,&moving,sprites,blend,add,NULL);
        reference_scene_draw(b,&moving,sprites,blend,add,NULL);
        if(memcmp(a,b,480000)) return 8;
        moving.camera_x+=6400; moving.camera_y+=4800;
        scene_draw(b,&moving,sprites,blend,add,NULL);
        if(memcmp(a,b,480000)) return 9;
    }
    printf("status=pass\ncamera_ticks_checked=2880\nscene_pixel_comparisons=%u\n",cases);
    for(i=0;i<6;++i) sprite_free(&sprites[i]);
    free(a); free(b); free(blend); free(add); return 0;
}
