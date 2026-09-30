/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "campaign.h"
#include "video.h"
#include <stdio.h>
#include <string.h>
void campaign_draw(unsigned char *frame,const Campaign *c,const Navigation *n)
{
 char text[128];unsigned i;int tons=0;
 for(i=0;i<c->state.cargo_count;++i)tons+=c->state.cargo[i].tons;
 memset(frame+24*800,10,800*9);
 snprintf(text,sizeof(text),"PILOT %.20s  CREDITS %lld  CARGO %d/%d T",
  c->state.name,(long long)c->state.credits,tons,(int)c->context.cargo_capacity);
 video_text(frame,12,24,text,6);
 for(i=565;i<574;++i)memset(frame+i*800,10,640);
 video_text(frame,12,565,c->notice,6);
 for(i=588;i<598;++i)memset(frame+i*800,10,800);
 video_text(frame,12,588,n->phase==NAV_DOCKED?"L DEPART  F5 SAVE  ESC EXIT":"W THRUST  A/D TURN  P PLANET  L APPROACH  TAB CAMERA  ESC EXIT",1);
 if(n->phase==NAV_DOCKED) {
  for(i=447;i<457;++i)memset(frame+i*800+62,10,676);
  video_text(frame,62,447,"L DEPART  /  F5 SAVE  /  ESC EXIT - SAVES ON LANDING",1);
 }
}
