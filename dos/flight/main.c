/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video.h"
#include "scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <dos.h>
#include <dpmi.h>

typedef struct { double sum,min,max; unsigned n; } Timing;
static double ms(void) { return (double)uclock()*1000./UCLOCKS_PER_SEC; }
static void sample(Timing *t,double value)
{
    t->sum+=value; if(!t->n || value<t->min) t->min=value;
    if(!t->n || value>t->max) t->max=value;
    ++t->n;
}
static void report(const char *name,const Timing *t)
{
    printf("%s_avg_ms=%.3f\n%s_min_ms=%.3f\n%s_max_ms=%.3f\n",name,
           t->n?t->sum/t->n:0.,name,t->min,name,t->max);
}
static unsigned long pages(void)
{
    __dpmi_free_mem_info m;
    return __dpmi_get_free_memory_information(&m)?0:m.total_number_of_free_pages;
}
static int load(const char *path,unsigned char *buffer,size_t n)
{
    FILE *f=fopen(path,"rb"); int ok;
    if(!f) return 0;
    ok=fread(buffer,1,n,f)==n && fgetc(f)==EOF && !ferror(f);
    fclose(f); return ok;
}
static int snapshot(const unsigned char *frame)
{
    FILE *f=fopen("FRAME.IDX","wb"); int ok;
    if(!f) return 0;
    ok=fwrite(frame,1,480000,f)==480000;
    if(fclose(f)) ok=0;
    return ok;
}
int main(int argc,char **argv)
{
    const char *names[]={"SPARROW.SPR","BARGE.SPR","FALCON.SPR","EARTH.SPR","LUNA.SPR","GLOW.SPR"};
    Sprite sprites[6]={{0}}; ActiveSystem system={0}; Scene scene;
    WorldStore *world=NULL;
    unsigned char palette[768],*frame=NULL,*blend=NULL,*add=NULL,*hud_pixels=NULL;
    Timing sim={0},draw={0},present={0},total={0};
    Timing background={0},orbital={0},traffic={0},effect={0},hud={0};
    unsigned frames=120,ships=6,i,done=0; int demo=0,ok=0,opened=0,camera=0;
    uint64_t id; size_t memory=0;
    unsigned long before=pages(),resident=0;
    double start=ms(),load_ms=0.,last=0.,accumulator=0.,seconds=20.,wall_start=0.,wall_elapsed=0.,discarded_ms=0.;
    const char *error="initialization_failed";
    for(i=1;i<(unsigned)argc;++i) {
        if(!strcmp(argv[i],"--demo")) demo=1;
        else if(!strcmp(argv[i],"--camera")) camera=1;
        else if(!strcmp(argv[i],"--frames") && i+1<(unsigned)argc) frames=(unsigned)atoi(argv[++i]);
        else if(!strcmp(argv[i],"--ships") && i+1<(unsigned)argc) ships=(unsigned)atoi(argv[++i]);
        else if(!strcmp(argv[i],"--seconds") && i+1<(unsigned)argc) seconds=atof(argv[++i]);
        else { error="unknown_argument"; goto done; }
    }
    if(!frames || frames>3600 || !ships || ships>64 || !(seconds>0. && seconds<=120.)) {
        error="argument_range"; goto done;
    }
    world=world_open("WORLD.PAK"); if(!world) { error="world_open_failed"; goto done; }
    if(!world_find_system(world,"Sol",&id) || !world_load_system(world,id,&system)) {
        error=world_error(world); goto done;
    }
    memory=world_store_bytes(world)+system.allocation_bytes;
    hud_pixels=malloc(56000);
    frame=malloc(480000); blend=malloc(1048576); add=malloc(65536);
    if(!hud_pixels || !frame || !blend || !add || !load("FLIGHT.PAL",palette,768) ||
       !load("BLEND.LUT",blend,1048576) || !load("ADD.LUT",add,65536)) goto done;
    memory+=480000+1048576+65536+56000;
    for(i=0;i<6;++i) {
        if(!sprite_load(&sprites[i],names[i])) { error="sprite_load_failed"; goto done; }
        memory+=sprites[i].bytes;
    }
    if(!scene_init(&scene,&system,ships)) { error="scene_requires_sol_earth_luna"; goto done; }
    /* These opaque labels never change in this prototype. Prepare once. */
    memset(frame,10,800*40); memset(frame+800*570,10,800*30);
    video_text(frame,12,10,"ENDLESS SKY DOS - SOL / EARTH",1);
    video_text(frame,12,24,"SCRIPTED FLIGHT PROTOTYPE - NOT GAMEPLAY",2);
    video_text(frame,12,578,"ESC EXIT    256 COLORS    60 HZ TEST MOTION",1);
    memcpy(hud_pixels,frame,32000);
    memcpy(hud_pixels+32000,frame+800*570,24000);
    scene.camera_enabled=camera;
    load_ms=ms()-start; resident=pages();
    if(!video_open(palette)) { error=video_error(); goto done; }
    opened=1; wall_start=last=ms();
    while((demo && ms()-wall_start<seconds*1000.) || (!demo && done<frames)) {
        double t=ms(),a,b,hud_start;
        SceneProfile profile;
        if(kbhit() && getch()==27) break;
        a=ms();
        if(demo) {
            accumulator+=a-last; last=a;
            /* Bounded real-time demo catchup: at most 15 ticks after a pause. */
            if(accumulator>250.) { discarded_ms+=accumulator-250.; accumulator=250.; }
            while(accumulator>=1000./60.) { scene_step(&scene); accumulator-=1000./60.; }
        } else { scene_step(&scene); scene_step(&scene); }
        b=ms(); sample(&sim,b-a); a=b;
        scene_draw(frame,&scene,sprites,blend,add,&profile);
        sample(&background,profile.background_ms); sample(&orbital,profile.orbital_ms);
        sample(&traffic,profile.traffic_ms); sample(&effect,profile.effect_ms);
        hud_start=ms();
        memcpy(frame,hud_pixels,32000);
        memcpy(frame+800*570,hud_pixels+32000,24000);
        b=ms(); sample(&hud,b-hud_start); sample(&draw,b-a); a=b;
        if(!video_present(frame)) { error=video_error(); goto done; }
        b=ms(); sample(&present,b-a); sample(&total,b-t); ++done;
        if(demo) while(ms()-t<1000./30.) delay(1);
    }
    wall_elapsed=ms()-wall_start;
    if(!done) { error="no_frames"; goto done; }
    if(!video_verify(frame)) { error=video_error(); goto done; }
    if(!snapshot(frame)) { error="snapshot_write_failed"; goto done; }
    ok=1;
done:
    if(opened) video_close();
    printf("status=%s\n",ok?"ok":"failed");
    if(!ok) printf("error=%s\n",error?error:"unknown");
    printf("mode=%s\nframes=%u\nships=%u\nload_ms=%.3f\nexplicit_resident_bytes=%lu\n"
           "free_pages_before=%lu\nfree_pages_resident=%lu\n",demo?"demo":"benchmark",done,
           ships,load_ms,(unsigned long)memory,before,resident);
    if(ok) printf("system=%s\nobjects=%lu\nsimulation_ticks=%u\nvideo_verify=pass\n",
                  system.name,(unsigned long)system.object_count,scene.ticks);
    printf("wall_elapsed_ms=%.3f\ndiscarded_sim_ms=%.3f\n",wall_elapsed,discarded_ms);
    printf("camera_enabled=%d\ncamera_x=%d\ncamera_y=%d\n",camera,
           ok?scene.camera_x:0,ok?scene.camera_y:0);
    report("background",&background); report("orbital",&orbital);
    report("traffic",&traffic); report("effect",&effect); report("hud",&hud);
    report("sim",&sim); report("draw",&draw); report("present",&present); report("frame",&total);
    for(i=0;i<6;++i) sprite_free(&sprites[i]);
    free(hud_pixels); free(frame); free(blend); free(add);
    world_release_system(&system); world_close(world);
    return ok?0:1;
}
