/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "state.h"
#include <float.h>
#include <string.h>

static const char *const commodities[PS_COMMODITIES] = {
 "commodity:Clothing", "commodity:Electronics", "commodity:Equipment",
 "commodity:Food", "commodity:Heavy Metals", "commodity:Industrial",
 "commodity:Luxury Goods", "commodity:Medical", "commodity:Metal",
 "commodity:Plastic"
};

static int nonzero_id(const uint8_t id[16])
{
 size_t i;
 for(i=0;i<16;i++) if(id[i]) return 1;
 return 0;
}

static int scalar(const unsigned char *p,size_t n)
{
 size_t i=0;
 while(i<n) {
  unsigned c=p[i++],cp,need,j,min;
  if(!c) return 0;
  if(c<0x80) continue;
  if(c>=0xc2 && c<=0xdf) { cp=c&31; need=1; min=0x80; }
  else if(c>=0xe0 && c<=0xef) { cp=c&15; need=2; min=0x800; }
  else if(c>=0xf0 && c<=0xf4) { cp=c&7; need=3; min=0x10000; }
  else return 0;
  if(need>n-i) return 0;
  for(j=0;j<need;j++) { c=p[i++]; if((c&0xc0)!=0x80) return 0; cp=(cp<<6)|(c&63); }
  if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
 }
 return 1;
}

static int text_ok(const char s[PS_TEXT],int empty)
{
 size_t n=0;
 while(n<PS_TEXT && s[n]) n++;
 return n<PS_TEXT && (empty || n) && scalar((const unsigned char *)s,n);
}

static int finite_value(double x) { return x==x && x<=DBL_MAX && x>=-DBL_MAX; }

static int leap(int32_t y) { return !(y%4) && ((y%100) || !(y%400)); }

static int valid_date(int32_t d,int32_t m,int32_t y)
{
 static const unsigned char days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 int lim;
 if(y<1 || m<1 || m>12 || d<1) return 0;
 lim=days[m-1]+(m==2 && leap(y));
 return d<=lim;
}

static int known_commodity(const char *s)
{
 size_t i;
 for(i=0;i<PS_COMMODITIES;i++) if(!strcmp(s,commodities[i])) return 1;
 return 0;
}

PSResult ps_validate(const PSState *s,const PSContext *c)
{
 uint32_t i;
 int64_t tons=0;
 if(!s || !c || c->cargo_capacity<0 || c->max_crew<0 ||
    !finite_value(c->max_fuel) || !finite_value(c->max_shields) ||
    !finite_value(c->max_hull) || c->max_fuel<0 || c->max_shields<0 || c->max_hull<0)
  return PS_INVALID;
 if(!text_ok(s->name,0) || !text_ok(s->system,0) || !text_ok(s->planet,0) ||
    !text_ok(s->model,0) || !text_ok(s->variant,1) || !text_ok(s->ship_name,0) ||
    !valid_date(s->day,s->month,s->year)) return PS_INVALID;
 if(s->scope_flags || strcmp(s->model,"ship:Sparrow") || s->variant[0]) return PS_UNSUPPORTED;
 if(strcmp(s->system,"system:Sol") ||
    (strcmp(s->planet,"planet:Earth") && strcmp(s->planet,"planet:Luna"))) return PS_UNSUPPORTED;
 if(!nonzero_id(s->pilot_id) || !nonzero_id(s->ship_id) ||
    !memcmp(s->pilot_id,s->ship_id,16) || memcmp(s->ship_id,s->flagship_id,16)) return PS_INVALID;
 if(s->crew<0 || s->crew>c->max_crew || !finite_value(s->fuel) ||
    !finite_value(s->shields) || !finite_value(s->hull) || s->fuel<0 ||
    s->fuel>c->max_fuel || s->shields<0 || s->shields>c->max_shields ||
    s->hull<0 || s->hull>c->max_hull || s->cargo_count>PS_COMMODITIES ||
    s->sale_count>PS_SALES) return PS_INVALID;
 for(i=0;i<s->cargo_count;i++) {
  const PSCargo *e=&s->cargo[i];
  if(!text_ok(e->key,0) || !known_commodity(e->key) || e->tons<0 ||
     (!e->tons && e->basis) || (i && strcmp(s->cargo[i-1].key,e->key)>=0)) return PS_INVALID;
  if(tons>INT64_MAX-e->tons) return PS_INVALID;
  tons+=e->tons;
 }
 if(tons>c->cargo_capacity) return PS_INVALID;
 for(i=0;i<s->sale_count;i++) {
  const PSSale *e=&s->sales[i];
  int cmp;
  if(!text_ok(e->system,0) || !text_ok(e->commodity,0) ||
     strcmp(e->system,"system:Sol") || !known_commodity(e->commodity) || e->delta>=0) return PS_INVALID;
  if(i) {
   cmp=strcmp(s->sales[i-1].system,e->system);
   if(cmp>0 || (!cmp && strcmp(s->sales[i-1].commodity,e->commodity)>=0)) return PS_INVALID;
  }
 }
 return PS_OK;
}

const char *ps_error(PSResult r)
{
 static const char *const names[]={"ok","missing","corrupt","unsupported","invalid","i/o error","conflict","stale","generation exhausted","out of memory"};
 return (unsigned)r<sizeof(names)/sizeof(names[0])?names[r]:"unknown error";
}
