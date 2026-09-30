/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "navigation_view.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SKY_TOP 36
#define SKY_BOTTOM 488
#define RADAR_X 646
#define RADAR_Y 505

static void box(unsigned char *frame,int x,int y,int w,int h,unsigned char color)
{
    int row;
    if(x<0 || y<0 || w<=0 || h<=0 || x+w>VIDEO_WIDTH || y+h>VIDEO_HEIGHT) return;
    for(row=0;row<h;++row) memset(frame+(size_t)(y+row)*VIDEO_WIDTH+x,color,(size_t)w);
}
static void dot(unsigned char *frame,int x,int y,unsigned char color)
{
    if(x>=0 && x<VIDEO_WIDTH && y>=SKY_TOP && y<SKY_BOTTOM)
        frame[(size_t)y*VIDEO_WIDTH+x]=color;
}
static void line(unsigned char *frame,int x0,int y0,int x1,int y1,unsigned char color)
{
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1;
    int error=dx+dy;
    for(;;) {
        if(x0>=0 && x0<VIDEO_WIDTH && y0>=0 && y0<VIDEO_HEIGHT)
            frame[(size_t)y0*VIDEO_WIDTH+x0]=color;
        if(x0==x1 && y0==y1) break;
        { int twice=2*error;
          if(twice>=dy) {error+=dy;x0+=sx;}
          if(twice<=dx) {error+=dx;y0+=sy;} }
    }
}
static int wrap(long long n,int period)
{
    int r=(int)(n%period);return r<0?r+period:r;
}
static void backdrop(unsigned char *frame,const Scene *scene)
{
    unsigned i,seed=17;
    memset(frame,0,VIDEO_PIXELS);
    for(i=0;i<230;++i) {
        int x,y;
        seed=seed*1664525u+1013904223u;
        x=wrap((long long)(seed%VIDEO_WIDTH)-scene->camera_x/8,VIDEO_WIDTH);
        seed=seed*1664525u+1013904223u;
        y=wrap((long long)(seed%VIDEO_HEIGHT)-scene->camera_y/8,VIDEO_HEIGHT);
        dot(frame,x,y,(unsigned char)(27+i%17));
    }
}
static int screen_xy(double wx,double wy,const Scene *scene,int margin,int *x,int *y)
{
    double sx=wx-(double)scene->camera_x,sy=wy-(double)scene->camera_y;
    if(!isfinite(sx)||!isfinite(sy)||sx<-(double)margin||
       sx>VIDEO_WIDTH+(double)margin||sy<-(double)margin||
       sy>VIDEO_HEIGHT+(double)margin) return 0;
    *x=(int)lround(sx);*y=(int)lround(sy);return 1;
}
static double fraction(double value,double maximum)
{
    double f=maximum>0.&&isfinite(value)?value/maximum:0.;
    return f<0.?0.:(f>1.?1.:f);
}
static void meter(unsigned char *frame,int x,int y,int width,const char *name,
                  double value,double maximum,unsigned char fill)
{
    char label[48];int used=(int)(fraction(value,maximum)*width+.5);
    box(frame,x,y+9,width,5,3);if(used) box(frame,x,y+9,used,5,fill);
    snprintf(label,sizeof(label),"%s %.0f/%.0f",name,value>0.?value:0.,maximum);
    video_text(frame,x,y,label,2);
}
static void tracer(unsigned char *frame,const Bolt *bolt,const Scene *scene)
{
    double ux,uy,x=bolt->x+.5*bolt->vx-(double)scene->camera_x;
    double y=bolt->y+.5*bolt->vy-(double)scene->camera_y;int k;
    if(!isfinite(x)||!isfinite(y)||x< -8.||x>808.||
       y<SKY_TOP-8.||y>SKY_BOTTOM+8.) return;
    motion_unit(bolt->angle,&ux,&uy);
    for(k=-3;k<=3;++k) dot(frame,(int)lround(x+k*ux),(int)lround(y+k*uy),k?7:1);
}
/* Frame pixels are palette indices, followed by 4-bit coverage for normal sprites.
 * Sample each destination pixel once: no scratch bitmap or heap allocation. */
static void draw_scaled(unsigned char *frame,const Sprite *sprite,unsigned heading,
                        int cx,int cy,float zoom,const unsigned char *blend,
                        const unsigned char *add)
{
    unsigned n,frame_no;const unsigned char *source;
    int w,h,x0,y0,x1,y1,x,y;
    if(!sprite->pixels||!sprite->frames||!sprite->width||!sprite->height||
       !isfinite(zoom)||zoom<=0.f) return;
    if(zoom>=.999f) {sprite_draw(frame,sprite,heading,cx,cy,blend,add);return;}
    w=(int)(sprite->width*zoom+.5f);h=(int)(sprite->height*zoom+.5f);
    if(w<1) w=1;
    if(h<1) h=1;
    if(w>VIDEO_WIDTH||h>VIDEO_HEIGHT) return;
    x0=cx-w/2;y0=cy-h/2;x1=x0+w;y1=y0+h;
    if(x1<=0||y1<=0||x0>=VIDEO_WIDTH||y0>=VIDEO_HEIGHT) return;
    n=sprite->width*sprite->height;frame_no=heading%sprite->frames;
    source=sprite->pixels+(size_t)frame_no*n*(sprite->mode?1:2);
    for(y=y0<0?0:y0;y<y1&&y<VIDEO_HEIGHT;++y) {
        unsigned sy=(unsigned)((size_t)(y-y0)*sprite->height/(unsigned)h);
        for(x=x0<0?0:x0;x<x1&&x<VIDEO_WIDTH;++x) {
            unsigned sx=(unsigned)((size_t)(x-x0)*sprite->width/(unsigned)w);
            unsigned offset=sy*sprite->width+sx;
            unsigned char color=source[offset];
            unsigned char *dst=frame+(size_t)y*VIDEO_WIDTH+x;
            if(sprite->mode) {if(color) *dst=add[((unsigned)color<<8)|*dst];}
            else {
                unsigned alpha=source[n+offset];
                if(alpha==15) *dst=color;
                else if(alpha) *dst=blend[(alpha<<16)|((unsigned)color<<8)|*dst];
            }
        }
    }
}
static void selected_marker(unsigned char *frame,const NavDestination *dest,
                            const Scene *scene,int visible)
{
    double sx=dest->x-(double)scene->camera_x;
    double sy=dest->y-(double)scene->camera_y;
    double dx=sx-400.,dy=sy-262.,scale=1e30;int x,y;
    if(!isfinite(dx)||!isfinite(dy)) return;
    if(visible&&sx>=20.&&sx<=780.&&sy>=56.&&sy<=466.) {
        x=(int)lround(sx);y=(int)lround(sy);
        line(frame,x-23,y-23,x-13,y-23,6);
        line(frame,x-23,y-23,x-23,y-13,6);
        line(frame,x+23,y-23,x+13,y-23,6);
        line(frame,x+23,y-23,x+23,y-13,6);
        line(frame,x-23,y+23,x-13,y+23,6);
        line(frame,x-23,y+23,x-23,y+13,6);
        line(frame,x+23,y+23,x+13,y+23,6);
        line(frame,x+23,y+23,x+23,y+13,6);
        return;
    }
    if(dx==0.&&dy==0.) return;
    if(dx>0.) scale=386./dx;
    if(dx<0.) scale=-386./dx;
    if(dy>0.&&212./dy<scale) scale=212./dy;
    if(dy<0.&&-212./dy<scale) scale=-212./dy;
    if(!(scale>0.)||!isfinite(scale)) return;
    x=(int)lround(400.+dx*scale);y=(int)lround(262.+dy*scale);
    if(x<14)x=14;
    if(x>786)x=786;
    if(y<50)y=50;
    if(y>474)y=474;
    line(frame,x-5,y,x,y-5,6);line(frame,x,y-5,x+5,y,6);
    line(frame,x+5,y,x,y+5,6);line(frame,x,y+5,x-5,y,6);
}
static void radar(unsigned char *frame,const Navigation *nav,const Pilot *pilot)
{
    const int x=RADAR_X,y=RADAR_Y,w=140,h=73,cx=x+w/2,cy=y+h/2;
    unsigned i;
    box(frame,x,y,w,h,3);box(frame,x+1,y+1,w-2,h-2,10);
    line(frame,cx,y+4,cx,y+h-5,11);
    line(frame,x+4,cy,x+w-5,cy,11);
    for(i=0;i<NAV_DESTINATIONS;++i) {
        double rx=(nav->destinations[i].x-pilot->state.x)/30.;
        double ry=(nav->destinations[i].y-pilot->state.y)/30.;int px,py;
        if(!isfinite(rx)||!isfinite(ry)) continue;
        if(rx>64.)rx=64.;
        if(rx< -64.)rx=-64.;
        if(ry>31.)ry=31.;
        if(ry< -31.)ry=-31.;
        px=cx+(int)lround(rx);py=cy+(int)lround(ry);
        if((int)i==nav->selected) {
            line(frame,px-4,py,px+4,py,6);line(frame,px,py-4,px,py+4,6);
        } else box(frame,px-2,py-2,5,5,4);
    }
    box(frame,cx-2,cy-2,5,5,7);
    video_text(frame,x+4,y+5,"SCOPE",2);
}
static const NavDestination *selection(const Navigation *nav)
{
    return nav->selected>=0&&nav->selected<NAV_DESTINATIONS?
           &nav->destinations[nav->selected]:NULL;
}
static const char *land_status(const Navigation *nav,const Pilot *pilot,
                               const Practice *practice,const Threat *player,
                               const NavDestination *dest)
{
    double range,speed;
    if(nav->phase==NAV_DOCKED) return "DOCKED";
    if(nav->phase==NAV_LANDING) return "LANDING";
    if(nav->phase==NAV_LAUNCHING) return "LAUNCHING";
    if(player->destroyed||player->disabled) return "SHIP DISABLED";
    if(!dest||(dest->flags&NAV_ALLOWED)==0) return "LANDING DENIED";
    if(practice->resources.overheated) return "SHIP OVERHEATED";
    range=hypot(dest->x-pilot->state.x,dest->y-pilot->state.y);
    speed=hypot(pilot->state.vx,pilot->state.vy);
    if(!isfinite(range)||range>=dest->radius) return "TOO FAR";
    if(!isfinite(speed)||speed>=1.) return "TOO FAST";
    if(navigation_can_land(nav,pilot,practice,player)) return "READY TO LAND";
    return "LANDING DENIED";
}
static void cockpit(unsigned char *frame,const Navigation *nav,const Practice *practice,
                    const Pilot *pilot,const Threat *player)
{
    char label[128];const NavDestination *dest=selection(nav);
    const char *status=land_status(nav,pilot,practice,player,dest);
    double range=dest?hypot(dest->x-pilot->state.x,dest->y-pilot->state.y):0.;
    box(frame,0,0,VIDEO_WIDTH,SKY_TOP,10);
    box(frame,0,SKY_BOTTOM,VIDEO_WIDTH,VIDEO_HEIGHT-SKY_BOTTOM,10);
    box(frame,0,SKY_TOP-1,VIDEO_WIDTH,1,7);
    box(frame,0,SKY_BOTTOM,VIDEO_WIDTH,1,7);
    video_text(frame,12,9,"ENDLESS SKY DOS  /  SOL NAVIGATION",1);
    video_text(frame,530,9,status,nav->phase==NAV_DOCKED?5:
               !strcmp(status,"READY TO LAND")?5:!strcmp(status,"LANDING DENIED")?4:6);
    video_text(frame,12,24,"SOL  /  EARTH AND LUNA",6);
    video_text(frame,220,24,pilot->follow?"CAMERA FOLLOW":"CAMERA FIXED",2);
    snprintf(label,sizeof(label),"SPEED %.0f",nav->phase==NAV_DOCKED?0.:hypot(pilot->state.vx,pilot->state.vy)*60.);
    video_text(frame,530,24,label,2);
    video_text(frame,12,495,"SPARROW",7);
    meter(frame,12,510,140,"SHIELD",practice->shield.shields,
          practice->shield_profile.capacity,7);
    meter(frame,167,510,140,"HULL",player->hull,player->max_hull,5);
    meter(frame,12,537,140,"ENERGY",practice->resources.energy,
          practice->resource_profile.capacity,8);
    meter(frame,167,537,140,"HEAT",practice->resources.heat,
          practice->resource_profile.max_heat,practice->resources.overheated?4:6);
    video_text(frame,12,565,player->destroyed?"DESTROYED":player->disabled?"HULL DISABLED":
               practice->resources.overheated?"OVERHEATED":
               practice->resources.energy<practice->resource_profile.shot_energy?"GUN LOW ENERGY":
               practice->cooldown?"RELOADING":"READY",player->disabled||practice->resources.overheated?4:5);
    video_text(frame,323,495,dest?dest->name:"NO DESTINATION",6);
    snprintf(label,sizeof(label),"RANGE %.0f  RADIUS %.0f",
             isfinite(range)?range:0.,dest?dest->radius:0.);
    video_text(frame,323,510,label,2);
    video_text(frame,323,529,status,!strcmp(status,"READY TO LAND")?5:6);
    video_text(frame,323,547,nav->phase==NAV_DOCKED?"PORT SERVICES COMPLETE":nav->phase!=NAV_FLYING?"CONTROLS SUSPENDED":nav->approach?"AUTO APPROACH ACTIVE":"MANUAL FLIGHT",nav->approach?7:2);
    if(nav->phase!=NAV_DOCKED&&nav->notice_ticks&&nav->notice[0]) {
        char notice[51];memcpy(notice,nav->notice,50);notice[50]=0;
        video_text(frame,323,565,notice,6);
    }
    radar(frame,nav,pilot);
    video_text(frame,12,588,nav->phase==NAV_DOCKED?"L DEPART  R RESET  ESC EXIT":"W THRUST  A/D TURN  SPACE FIRE  P PLANET  L APPROACH  TAB CAMERA  R RESET  ESC EXIT",1);
}
/* The 5x7 font has six-pixel cells. A line is never wider than max_chars. */
static int paragraph(unsigned char *frame,int x,int y,const char *source,
                     size_t source_size,int max_chars,int last_y,unsigned char color)
{
    size_t pos=0;char linebuf[104];
    while(pos<source_size&&source[pos]&&y<=last_y) {
        int count=0,last_space=-1;size_t start=pos;
        while(pos<source_size&&source[pos]&&source[pos]!='\n'&&count<max_chars) {
            if(source[pos]==' '||source[pos]=='\t') last_space=count;
            ++pos;++count;
        }
        if(count==max_chars&&pos<source_size&&source[pos]&&source[pos]!='\n'&&last_space>0) {
            pos=start+(size_t)last_space+1;count=last_space;
        }
        while(count>0&&(source[start+(size_t)count-1]==' '||
                        source[start+(size_t)count-1]=='\t')) --count;
        memcpy(linebuf,source+start,(size_t)count);linebuf[count]=0;
        video_text(frame,x,y,linebuf,color);y+=12;
        while(pos<source_size&&(source[pos]==' '||source[pos]=='\t')) ++pos;
        if(pos<source_size&&source[pos]=='\n') ++pos;
    }
    return y;
}
static void docked(unsigned char *frame,const Navigation *nav,
                   const Practice *practice,const Pilot *pilot,const Threat *player)
{
    const NavDestination *dest=selection(nav);int y=101;
    char label[96];unsigned flags=dest?dest->flags:0;
    box(frame,46,57,708,411,3);box(frame,48,59,704,407,10);
    box(frame,48,59,704,24,3);
    snprintf(label,sizeof(label),"DOCKED  /  %s",dest?dest->name:"UNKNOWN PORT");
    video_text(frame,62,68,label,1);
    if(dest) y=paragraph(frame,62,y,dest->description,sizeof(dest->description),
                         103,254,2);
    box(frame,62,270,676,1,7);
    video_text(frame,62,281,"AVAILABLE PORT FACILITIES",7);
    y=300;
    if(flags&NAV_SPACEPORT) {video_text(frame,62,y,"SPACEPORT",2);y+=14;}
    if((flags&NAV_SERVICES)&&(flags&NAV_REPAIR_HULL))
        {video_text(frame,62,y,"HULL REPAIR",5);y+=14;}
    if((flags&NAV_SERVICES)&&(flags&NAV_RECHARGE_SHIELD))
        {video_text(frame,62,y,"SHIELD RECHARGE",5);y+=14;}
    if((flags&NAV_SERVICES)&&(flags&NAV_RECHARGE_ENERGY))
        {video_text(frame,62,y,"ENERGY RECHARGE",5);y+=14;}
    if(!(flags&NAV_SPACEPORT)&&!(flags&NAV_SERVICES &&
         (flags&(NAV_REPAIR_HULL|NAV_RECHARGE_SHIELD|NAV_RECHARGE_ENERGY))))
        video_text(frame,62,y,"NO ACTIVE FACILITIES",6);
    if(flags&NAV_OUTFITTER) video_text(frame,360,300,"OUTFITTER (INFO ONLY)",2);
    if(flags&NAV_SHIPYARD) video_text(frame,360,314,"SHIPYARD (INFO ONLY)",2);
    video_text(frame,62,386,"TRADING / OUTFITTING / SHIPYARD / MISSIONS: UNAVAILABLE",6);
    snprintf(label,sizeof(label),"SPARROW  SHIELD %.0f/%.0f  HULL %.0f/%.0f",
             practice->shield.shields,practice->shield_profile.capacity,
             player->hull,player->max_hull);
    video_text(frame,62,411,label,2);
    snprintf(label,sizeof(label),"ENERGY %.0f/%.0f  HEAT %.0f/%.0f",
             practice->resources.energy,practice->resource_profile.capacity,
             practice->resources.heat,practice->resource_profile.max_heat);
    video_text(frame,62,425,label,2);
    video_text(frame,62,447,"L DEPART  /  R RESET  /  ESC EXIT",1);
    (void)pilot;
}
void navigation_view_draw(unsigned char *frame,const Navigation *nav,
                          const Practice *practice,const Pilot *pilot,
                          const Threat *player,const Scene *scene,
                          const Sprite sprites[6],const unsigned char *blend,
                          const unsigned char *add)
{
    unsigned i;const NavDestination *dest=selection(nav);
    backdrop(frame,scene);
    for(i=0;i<NAV_DESTINATIONS;++i) {
        const NavDestination *planet=&nav->destinations[i];int x,y;
        if(screen_xy(planet->x,planet->y,scene,260,&x,&y))
            sprite_draw(frame,&sprites[i+3],0,x,y,blend,add);
    }
    if(dest) {
        int x,y,visible=screen_xy(dest->x,dest->y,scene,260,&x,&y);
        selected_marker(frame,dest,scene,visible);
    }
    for(i=0;i<PRACTICE_BOLTS;++i)
        if(practice->bolts[i].alive) tracer(frame,&practice->bolts[i],scene);
    if(!player->destroyed&&nav->phase!=NAV_DOCKED) {
        int x,y;
        if(screen_xy(pilot->state.x,pilot->state.y,scene,100,&x,&y))
            draw_scaled(frame,&sprites[0],
                        ((unsigned)pilot->state.angle*sprites[0].frames+32768u)/65536u,
                        x,y,nav->zoom,blend,add);
    }
    cockpit(frame,nav,practice,pilot,player);
    if(nav->phase==NAV_DOCKED) docked(frame,nav,practice,pilot,player);
}
