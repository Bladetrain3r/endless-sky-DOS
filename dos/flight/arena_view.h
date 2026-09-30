/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_ARENA_VIEW_H
#define FLIGHT_ARENA_VIEW_H
#include "arena.h"
#include "scene.h"
#include "sprite.h"
/* Complete 800x600 indexed arena frame; owns no buffers or simulation state. */
void arena_view_draw(unsigned char *frame,const Arena *arena,const Practice *practice,
                     const Pilot *pilot,const Threat *player,const Scene *scene,
                     const Sprite sprites[6],const unsigned char *blend,
                     const unsigned char *add);
#endif
