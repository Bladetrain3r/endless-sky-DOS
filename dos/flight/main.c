/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "video.h"
#include "scene.h"
#include "pilot.h"
#include "input.h"
#include "practice.h"
#include "power.h"
#include "threat.h"
#include <stdio.h>
#include <math.h>
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
    Sprite sprites[6]={{0}}; ActiveSystem system={0}; Scene scene; Pilot pilot={0};
    WorldStore *world=NULL; Practice *practice=NULL; Threat *threat=NULL; PropulsionProfile drive={0};
    unsigned char palette[768],*frame=NULL,*blend=NULL,*add=NULL,*hud_pixels=NULL;
    Timing sim={0},draw={0},present={0},total={0};
    Timing background={0},orbital={0},traffic={0},effect={0},hud={0},pilot_draw_time={0};
    unsigned frames=120,ships=6,i,done=0; int demo=0,ok=0,opened=0,camera=0;
    int controlled=0,replay=0,keyboard=0,practice_test=0,stationary_target=0,invulnerable_target=0;
    int resource_stress=0,incoming_fire=0,incoming_test=0;
    unsigned keys_seen=0;
    uint64_t id; size_t memory=0;
    unsigned long before=pages(),resident=0;
    double start=ms(),load_ms=0.,last=0.,accumulator=0.,seconds=20.,wall_start=0.,wall_elapsed=0.,discarded_ms=0.;
    const char *error="initialization_failed";
    for(i=1;i<(unsigned)argc;++i) {
        if(!strcmp(argv[i],"--demo")) demo=1;
        else if(!strcmp(argv[i],"--incoming-fire")) incoming_fire=1;
        else if(!strcmp(argv[i],"--incoming-test")) controlled=replay=incoming_fire=incoming_test=1;
        else if(!strcmp(argv[i],"--invulnerable-target")) invulnerable_target=1;
        else if(!strcmp(argv[i],"--stationary-target")) stationary_target=1;
        else if(!strcmp(argv[i],"--resource-stress") && i+1<(unsigned)argc) {
            const char *mode=argv[++i];
            if(!strcmp(mode,"energy")) resource_stress=1;
            else if(!strcmp(mode,"heat")) resource_stress=2;
            else { error="resource_stress_mode"; goto done; }
        }
        else if(!strcmp(argv[i],"--practice-test")) controlled=replay=practice_test=1;
        else if(!strcmp(argv[i],"--pilot")) controlled=1;
        else if(!strcmp(argv[i],"--replay")) controlled=replay=1;
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
    if(controlled) {
        if(!pilot_load(&pilot,"PILOT.DAT")) { error="pilot_profile_failed"; goto done; }
        pilot_reset(&pilot,&scene);
        practice=malloc(sizeof(*practice));
        threat=malloc(sizeof(*threat));
        if(!threat || !threat_load(threat,"PLAYER.DAT") || !practice || !practice_load(practice,"BLASTER.DAT","MASKS.BIN") ||
           !practice_damage_load(practice,"DAMAGE.DAT") ||
           !practice_resources_load(practice,"RESOURCE.DAT",resource_stress) ||
           !practice_shields_load(practice,"SHIELD.DAT") ||
           !propulsion_load(&drive,"PROPULSE.DAT")) {
            error="practice_assets_failed"; goto done;
        }
        threat->enabled=incoming_fire;
        practice->moving_target=!stationary_target;
        practice->destructible=!invulnerable_target;
        practice_reset(practice);
        if(incoming_test) for(i=0;i<4;++i) practice_shield_test(practice);
        memory+=sizeof(*practice)+sizeof(*threat);
    }
    /* These opaque labels never change in this prototype. Prepare once. */
    memset(frame,10,800*40); memset(frame+800*570,10,800*30);
    video_text(frame,12,10,"ENDLESS SKY DOS - SOL / EARTH",1);
    video_text(frame,12,24,controlled?(resource_stress==1?"TRAINER: ENERGY STRESS - SHARED POWER":
                resource_stress==2?"TRAINER: HEAT STRESS - SHARED POWER":
                (incoming_fire?"TRAINER: INCOMING FIRE":"TRAINER: SHARED ENERGY / HEAT")):
               "SCRIPTED FLIGHT PROTOTYPE - NOT GAMEPLAY",2);
    video_text(frame,12,578,controlled?"W/UP THRUST  A/D TURN  SPACE FIRE  TAB CAMERA  R RESET  H SHIELD TEST  ESC EXIT":
               "ESC EXIT    256 COLORS    60 HZ TEST MOTION",1);
    memcpy(hud_pixels,frame,32000);
    memcpy(hud_pixels+32000,frame+800*570,24000);
    scene.camera_enabled=camera && !controlled;
    load_ms=ms()-start; resident=pages();
    if(!video_open(palette)) { error=video_error(); goto done; }
    opened=1;
    if(controlled && !replay) {
        if(!input_open()) { error="keyboard_open_failed"; goto done; }
        keyboard=1;
    }
    wall_start=last=ms();
    while((demo && ms()-wall_start<seconds*1000.) || (!demo && done<frames)) {
        double t=ms(),a,b,hud_start;
        SceneProfile profile;
        if(keyboard) {
            unsigned keys=input_keys();
            keys_seen|=keys;
            if(keys & INPUT_EXIT) break;
            if((keys & ~pilot.previous_keys) & INPUT_RESET) {
                practice_reset(practice);threat_reset(threat);
            }
            if(!threat->destroyed && ((keys & ~pilot.previous_keys) & INPUT_SHIELD_TEST)) practice_shield_test(practice);
            pilot_keys(&pilot,&scene,keys);
        } else if(kbhit() && getch()==27) break;
        a=ms();
        if(demo) {
            accumulator+=a-last; last=a;
            /* Bounded real-time demo catchup: at most 15 ticks after a pause. */
            if(accumulator>250.) { discarded_ms+=accumulator-250.; accumulator=250.; }
        } else accumulator=2000./60.;
        while(accumulator>=1000./60.) {
            scene_step(&scene);
            if(controlled) {
                if(replay) pilot_keys(&pilot,&scene,incoming_test?0:(practice_test?INPUT_FIRE:pilot_replay_keys(pilot.ticks)));
                practice_begin_tick_disabled(practice,threat->disabled);
                if(threat->destroyed) ++pilot.ticks; /* Trainer death freezes position until R. */
                else if(threat->disabled) pilot_disabled_step(&pilot,&scene);
                else pilot_powered_step(&pilot,&scene,&practice->resources,&practice->resource_profile,&drive);
                practice_finish_tick(practice,&pilot,!threat->disabled && !!(pilot.previous_keys & INPUT_FIRE));
                threat_step(threat,practice,&pilot);
            }
            accumulator-=1000./60.;
        }
        b=ms(); sample(&sim,b-a); a=b;
        scene_draw(frame,&scene,sprites,blend,add,&profile);
        if(controlled) {
            double pilot_start=ms();
            practice_draw(frame,practice,&scene,&sprites[1],blend,add);
            threat_draw(frame,threat,&scene);
            if(!threat->destroyed) pilot_draw(frame,&pilot,&scene,&sprites[0],blend,add);
            sample(&pilot_draw_time,ms()-pilot_start);
        }
        sample(&background,profile.background_ms); sample(&orbital,profile.orbital_ms);
        sample(&traffic,profile.traffic_ms); sample(&effect,profile.effect_ms);
        hud_start=ms();
        memcpy(frame,hud_pixels,32000);
        memcpy(frame+800*570,hud_pixels+32000,24000);
        if(controlled) {
            char label[100];
            snprintf(label,sizeof(label),"SPEED %.0f  CAMERA %s  NO REVERSE ENGINES",
                     sqrt(pilot.state.vx*pilot.state.vx+pilot.state.vy*pilot.state.vy)*60.,
                     pilot.follow?"FOLLOW":"FIXED");
            video_text(frame,420,10,label,1);
            snprintf(label,sizeof(label),"SHOTS %u  HITS %u  ACTIVE %u",practice->shots,practice->hits,practice->active);
            video_text(frame,490,24,label,1);
            snprintf(label,sizeof(label),"ENERGY %.0f/%.0f  HEAT %.0f%%  %s",
                     practice->resources.energy,practice->resource_profile.capacity,
                     100.*practice->resources.heat/practice->resource_profile.max_heat,
                     threat->destroyed?"DESTROYED":threat->disabled?"HULL DISABLED":
                     practice->resources.overheated?"OVERHEATED":
                     practice->resources.energy<practice->resource_profile.shot_energy?"GUN LOW ENERGY":
                     practice->cooldown?"RELOADING":"READY");
            video_text(frame,12,590,label,practice->resources.overheated?4:2);
            if(practice->destructible)
                snprintf(label,sizeof(label),"TARGET SH %.1f  HULL %.1f%s",practice->health.shields,
                    practice->health.hull>0.?practice->health.hull:0.,practice->destroyed?"  DESTROYED":"");
            else snprintf(label,sizeof(label),"TARGET INVULNERABLE");
            video_text(frame,490,590,label,1);
            snprintf(label,sizeof(label),"SHIELD %.1f/%.0f  HULL %.1f/%.0f  H: DRAIN 25%% (TRAINER)",
                     practice->shield.shields,practice->shield_profile.capacity,
                     threat->hull>0.?threat->hull:0.,threat->max_hull);
            video_text(frame,12,44,label,threat->flash?4:2);
            if(threat->destroyed) video_text(frame,250,280,"SHIP DESTROYED - R TO RESET",4);
            else if(threat->disabled) video_text(frame,250,280,"HULL DISABLED - R TO RESET",4);
        }
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
    if(keyboard) input_close();
    if(opened) video_close();
    printf("status=%s\n",ok?"ok":"failed");
    if(!ok) printf("error=%s\n",error?error:"unknown");
    printf("mode=%s\nframes=%u\nships=%u\nload_ms=%.3f\nexplicit_resident_bytes=%lu\n"
           "free_pages_before=%lu\nfree_pages_resident=%lu\n",demo?"demo":"benchmark",done,
           ships,load_ms,(unsigned long)memory,before,resident);
    if(ok) printf("system=%s\nobjects=%lu\nsimulation_ticks=%u\nvideo_verify=pass\n",
                  system.name,(unsigned long)system.object_count,scene.ticks);
    printf("wall_elapsed_ms=%.3f\ndiscarded_sim_ms=%.3f\n",wall_elapsed,discarded_ms);
    printf("camera_enabled=%d\ncamera_x=%d\ncamera_y=%d\n",ok?scene.camera_enabled:0,
           ok?scene.camera_x:0,ok?scene.camera_y:0);
    if(controlled && ok) printf("pilot_ticks=%u\npilot_x=%.17g\npilot_y=%.17g\n"
        "pilot_vx=%.17g\npilot_vy=%.17g\npilot_angle=%u\npilot_follow=%d\n",
        pilot.ticks,pilot.state.x,pilot.state.y,pilot.state.vx,pilot.state.vy,
        (unsigned)pilot.state.angle,pilot.follow);
    if(controlled) printf("pilot_input_bits=%u\npilot_keyboard=%d\n",keys_seen,keyboard);
    if(practice && ok) printf("practice_shots=%u\npractice_hits=%u\npractice_active=%u\n"
        "practice_peak=%u\npractice_dropped=%u\n",practice->shots,practice->hits,
        practice->active,practice->peak,practice->dropped);
    if(practice && ok) printf("target_moving=%d\ntarget_ticks=%u\ntarget_x=%.17g\ntarget_y=%.17g\ntarget_angle=%u\n",
        practice->moving_target,practice->target_ticks,practice->target.x,practice->target.y,(unsigned)practice->target.angle);
    if(practice && ok) printf("target_destroyed=%d\ntarget_shields=%.17g\ntarget_hull=%.17g\nexplosion_ticks=%u\n",
        practice->destroyed,practice->health.shields,practice->health.hull,practice->explosion);
    if(practice && ok) printf("resource_stress=%d\nenergy=%.17g\nheat=%.17g\noverheated=%d\n"
        "blocked_energy_ticks=%u\nblocked_heat_ticks=%u\n",
        resource_stress,practice->resources.energy,practice->resources.heat,practice->resources.overheated,
        practice->blocked_energy,practice->blocked_heat);
    if(practice && ok) printf("player_shields=%.17g\nplayer_shield_capacity=%.17g\nshield_test_pulses=%u\n",
        practice->shield.shields,practice->shield_profile.capacity,practice->shield_test_pulses);
    if(threat && ok) printf("incoming_fire=%d\nenemy_shots=%u\nenemy_hits=%u\nenemy_active=%u\nenemy_dropped=%u\n"
        "player_hull=%.17g\nplayer_minimum_hull=%.17g\nplayer_disabled=%d\nplayer_destroyed=%d\n",
        threat->enabled,threat->shots,threat->hits,threat->active,threat->dropped,
        threat->hull,threat->minimum_hull,threat->disabled,threat->destroyed);
    report("background",&background); report("orbital",&orbital);
    report("traffic",&traffic); report("effect",&effect); report("hud",&hud);
    if(controlled) report("pilot_draw",&pilot_draw_time);
    report("sim",&sim); report("draw",&draw); report("present",&present); report("frame",&total);
    for(i=0;i<6;++i) sprite_free(&sprites[i]);
    free(threat); free(practice); free(hud_pixels); free(frame); free(blend); free(add);
    world_release_system(&system); world_close(world);
    return ok?0:1;
}
