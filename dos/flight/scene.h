/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_SCENE_H
#define FLIGHT_SCENE_H
#include "sprite.h"
#include "../world/active.h"
typedef struct { int x,y; unsigned heading,kind; } Traffic;
typedef struct {
    Traffic ships[64]; unsigned count,ticks;
    int earth_x,earth_y,luna_x,luna_y;
    int camera_enabled,camera_x,camera_y;
} Scene;
typedef struct {
    double background_ms,orbital_ms,traffic_ms,effect_ms;
} SceneProfile;
int scene_init(Scene *scene,const ActiveSystem *system,unsigned ships);
void scene_step(Scene *scene);
void scene_draw(unsigned char *frame,const Scene *scene,const Sprite sprites[6],
                const unsigned char *blend,const unsigned char *add,SceneProfile *profile);
#endif
