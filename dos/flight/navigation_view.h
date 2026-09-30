/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_NAVIGATION_VIEW_H
#define FLIGHT_NAVIGATION_VIEW_H
#include "navigation.h"
#include "sprite.h"
/* Complete 800x600 indexed navigation frame; owns no state or buffers. */
void navigation_view_draw(unsigned char *frame,const Navigation *nav,
                          const Practice *practice,const Pilot *pilot,
                          const Threat *player,const Scene *scene,
                          const Sprite sprites[6],const unsigned char *blend,
                          const unsigned char *add);
#endif
