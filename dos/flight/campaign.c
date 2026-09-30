/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "campaign.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "SAVEKEY.H"
static void notice(Campaign *c,const char *message)
{
 snprintf(c->notice,sizeof(c->notice),"%s",message);++c->revision;
}
static int cargo_tons(const PSState *s)
{
 unsigned i;int n=0;for(i=0;i<s->cargo_count;++i)n+=s->cargo[i].tons;return n;
}
static PSResult new_pilot(Campaign *c)
{
 char path[160];FILE *f;unsigned char ids[32];int ok;
 snprintf(path,sizeof(path),"%s/SEED.DAT",c->directory);
 f=fopen(path,"rb");if(!f)return PS_IO;
 ok=fread(ids,1,sizeof(ids),f)==sizeof(ids)&&fgetc(f)==EOF&&!ferror(f);
 if(fclose(f))ok=0;
 if(!ok)return PS_INVALID;
 memset(&c->state,0,sizeof(c->state));
 memcpy(c->state.pilot_id,ids,16);memcpy(c->state.ship_id,ids+16,16);
 memcpy(c->state.flagship_id,ids+16,16);
 strcpy(c->state.name,"First Pilot");strcpy(c->state.ship_name,"First Light");
 strcpy(c->state.system,"system:Sol");strcpy(c->state.planet,"planet:Earth");
 strcpy(c->state.model,"ship:Sparrow");
 c->state.day=campaign_date[0];c->state.month=campaign_date[1];c->state.year=campaign_date[2];
 c->state.credits=10000;c->state.crew=campaign_crew;
 c->state.fuel=c->context.max_fuel;c->state.shields=c->context.max_shields;c->state.hull=c->context.max_hull;
 return ps_validate(&c->state,&c->context);
}
PSResult campaign_open(Campaign *c,const char *directory,Navigation *n,Pilot *p,
                       Scene *scene,Practice *practice,Threat *threat)
{
 PSResult result;int tons;const double *motion;unsigned flags;
 memset(c,0,sizeof(*c));c->context=campaign_context;ps_store_session_init(&c->session);
 if(!directory || strlen(directory)>=sizeof(c->directory))return PS_INVALID;
 strcpy(c->directory,directory);
 /* Reject a mismatched runtime instead of relabeling a trainer as a stock save. */
 if(practice->shield_profile.capacity!=c->context.max_shields || threat->max_hull!=c->context.max_hull)
  return PS_UNSUPPORTED;
 result=ps_store_load(directory,&c->context,&c->state,&c->session);
 if(result==PS_MISSING) {result=new_pilot(c);c->dirty=1;}
 if(result!=PS_OK)return result;
 tons=cargo_tons(&c->state);motion=cargo_profiles[tons];
 p->parameters=(MotionParameters){motion[0],motion[1],motion[2],motion[3],motion[4]};
 practice->resource_profile.max_heat=motion[5];n->idle_heat=motion[6];
 n->selected=!strcmp(c->state.planet,"planet:Luna");
 n->phase=NAV_DOCKED;n->zoom=0.f;n->approach=0;n->release_controls=1;
 p->state=(MotionState){n->destinations[n->selected].x,n->destinations[n->selected].y,0.,0.,0};
 p->command=(MotionCommand){0};p->previous_keys=0;p->follow=1;
 scene->camera_x=(int)lround(p->state.x)-400;scene->camera_y=(int)lround(p->state.y)-300;
 flags=n->destinations[n->selected].flags;
 /* Match the bounded native landed normalization, not in-flight snapshots. */
 practice->shield.shields=c->state.shields;threat->hull=c->state.hull;
 if(flags & NAV_SERVICES) {
  if(flags & NAV_RECHARGE_SHIELD)shield_reset(&practice->shield,&practice->shield_profile);
  if(flags & NAV_RECHARGE_ENERGY)practice->resources.energy=practice->resource_profile.capacity;
  if(flags & NAV_REPAIR_HULL)threat->hull=threat->max_hull;
  c->state.fuel=c->context.max_fuel;
  if(c->state.crew<campaign_crew)c->state.crew=campaign_crew;
 }
 practice->resources.heat=n->idle_heat;practice->resources.overheated=0;
 memset(practice->bolts,0,sizeof(practice->bolts));practice->active=practice->cooldown=0;
 threat->disabled=threat->hull<threat->minimum_hull;threat->destroyed=threat->hull<0.;
 c->state.shields=practice->shield.shields;c->state.hull=threat->hull;
 if(!c->session.generation)return campaign_save(c,n,practice,threat);
 notice(c,c->session.recovered?"RECOVERED LAST GOOD SAVE":"PILOT LOADED");return PS_OK;
}
PSResult campaign_save(Campaign *c,Navigation *n,const Practice *practice,const Threat *threat)
{
 PSResult result;
 if(n->phase!=NAV_DOCKED || threat->destroyed) {
  ++c->rejections;notice(c,"LAND BEFORE SAVING");return PS_INVALID;
 }
 /* Candidate publication follows successful disk validation; dirty live state
  * remains retryable on I/O failure. No economic action is retried here. */
 c->dirty=1;
 snprintf(c->state.planet,sizeof(c->state.planet),"planet:%s",n->destinations[n->selected].name);
 c->state.shields=practice->shield.shields;c->state.hull=threat->hull;
 result=ps_store_save(c->directory,&c->context,&c->state,&c->session);
 if(result==PS_OK) {c->dirty=0;++c->saves;notice(c,"PILOT SAVED");}
 else {char message[80];snprintf(message,sizeof(message),"SAVE FAILED: %s",ps_error(result));notice(c,message);}
 return result;
}
unsigned campaign_keys(Campaign *c,Navigation *n,const Practice *practice,
                       const Threat *threat,unsigned keys,unsigned previous)
{
 unsigned pressed=keys & ~c->previous_keys;
 (void)previous;c->previous_keys=keys;
 if(!(keys & INPUT_LAND))c->block_depart=0;
 if(pressed & INPUT_RESET)notice(c,"RESET IS FOR TRAINERS - PILOT RETAINED");
 if((pressed & INPUT_SAVE) && !((pressed & INPUT_LAND) && n->phase==NAV_DOCKED))
  campaign_save(c,n,practice,threat);
 if((pressed & INPUT_LAND) && n->phase==NAV_DOCKED) {
  if(campaign_save(c,n,practice,threat)!=PS_OK)c->block_depart=1;
  else c->dirty=1;
 }
 /* This mode owns a stock loadout; the trainer blaster is not that loadout. */
 if(c->block_depart)keys&=~INPUT_LAND;
 return keys & ~(INPUT_RESET|INPUT_SHIELD_TEST|INPUT_FIRE);
}
void campaign_landed(Campaign *c,Navigation *n,const Practice *practice,const Threat *threat)
{
 c->state.fuel=c->context.max_fuel;
 campaign_save(c,n,practice,threat);
}
void campaign_report(const Campaign *c)
{
 unsigned i;
 printf("campaign=1\nsave_generation=%llu\nsave_recovered=%d\nsave_dirty=%d\nsave_count=%u\nsave_rejections=%u\n",
  (unsigned long long)c->session.generation,c->session.recovered,c->dirty,c->saves,c->rejections);
 printf("saved_planet=%s\npilot_credits=%lld\npilot_cargo=%d\npilot_date=%d-%d-%d\npilot_id=",
  c->state.planet,(long long)c->state.credits,cargo_tons(&c->state),(int)c->state.year,(int)c->state.month,(int)c->state.day);
 for(i=0;i<16;++i)printf("%02x",c->state.pilot_id[i]);
 printf("\nship_id=");for(i=0;i<16;++i)printf("%02x",c->state.ship_id[i]);
 printf("\nsave_notice=%s\nsave_heap_peak_bound=%lu\n",c->notice,
  (unsigned long)PS_STORE_SAVE_HEAP_MAX);
}
