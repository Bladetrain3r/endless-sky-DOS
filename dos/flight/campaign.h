/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_CAMPAIGN_H
#define FLIGHT_CAMPAIGN_H
#include "navigation.h"
#include "../persistence/store.h"
typedef struct {
 PSState state;
 PSSession session;
 PSContext context;
 char directory[128],notice[80];
 unsigned revision,saves,rejections;
 int dirty,block_depart;
 unsigned previous_keys;
} Campaign;
PSResult campaign_open(Campaign *c,const char *directory,Navigation *nav,Pilot *pilot,
                       Scene *scene,Practice *practice,Threat *threat);
PSResult campaign_save(Campaign *c,Navigation *nav,const Practice *practice,const Threat *threat);
/* Filters reset/test/fire and saves before departure. Call before navigation_keys. */
unsigned campaign_keys(Campaign *c,Navigation *nav,const Practice *practice,
                       const Threat *threat,unsigned keys,unsigned previous);
void campaign_landed(Campaign *c,Navigation *nav,const Practice *practice,const Threat *threat);
void campaign_draw(unsigned char *frame,const Campaign *c,const Navigation *nav);
void campaign_report(const Campaign *c);
#endif
