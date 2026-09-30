/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "arena_view.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SKY_TOP 36
#define SKY_BOTTOM 488
#define ARENA_PI 3.14159265358979323846

static void dot(unsigned char *frame,int x,int y,unsigned char color)
{
    if(x>=0 && x<VIDEO_WIDTH && y>=SKY_TOP && y<SKY_BOTTOM)
        frame[y*VIDEO_WIDTH+x]=color;
}
static void line(unsigned char *frame,int x0,int y0,int x1,int y1,unsigned char color)
{
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1;
    int error=dx+dy;
    for(;;) {
        if(x0>=0 && x0<VIDEO_WIDTH && y0>=0 && y0<VIDEO_HEIGHT)
            frame[y0*VIDEO_WIDTH+x0]=color;
        if(x0==x1 && y0==y1) break;
        { int twice=2*error;
          if(twice>=dy) { error+=dy;x0+=sx; }
          if(twice<=dx) { error+=dx;y0+=sy; } }
    }
}
static void box(unsigned char *frame,int x,int y,int w,int h,unsigned char color)
{
    int row;
    if(x<0 || y<0 || w<1 || h<1 || x+w>VIDEO_WIDTH || y+h>VIDEO_HEIGHT) return;
    for(row=0;row<h;++row) memset(frame+(y+row)*VIDEO_WIDTH+x,color,(size_t)w);
}
static int screen_xy(double wx,double wy,const Scene *scene,int margin,int *x,int *y)
{
    double sx=wx-(double)scene->camera_x,sy=wy-(double)scene->camera_y;
    if(!isfinite(sx) || !isfinite(sy) || sx<-(double)margin ||
       sx>VIDEO_WIDTH+(double)margin || sy<-(double)margin ||
       sy>VIDEO_HEIGHT+(double)margin) return 0;
    *x=(int)lround(sx);*y=(int)lround(sy);return 1;
}
static int wrap(long long x,int n)
{
    int r=(int)(x%n);return r<0?r+n:r;
}
static void backdrop(unsigned char *frame,const Scene *scene)
{
    unsigned i;unsigned seed=17;
    memset(frame,0,VIDEO_PIXELS);
    for(i=0;i<230;++i) {
        int x,y;
        seed=seed*1664525u+1013904223u;
        x=wrap((long long)(seed%VIDEO_WIDTH)-scene->camera_x/8,VIDEO_WIDTH);
        seed=seed*1664525u+1013904223u;
        y=wrap((long long)(seed%VIDEO_HEIGHT)-scene->camera_y/8,VIDEO_HEIGHT);
        dot(frame,x,y,(unsigned char)(27+i%17));
    }
    /* Navigation buoys, visual only: six fixed points about 900 units out. */
    for(i=0;i<6;++i) {
        double angle=(double)i*2.*ARENA_PI/6.;int x,y;
        if(screen_xy(400.+900.*sin(angle),300.-900.*cos(angle),scene,12,&x,&y)) {
            line(frame,x-5,y,x+5,y,8);line(frame,x,y-5,x,y+5,8);
            dot(frame,x,y,1);
        }
    }
}
static void spark(unsigned char *frame,int x,int y,unsigned life)
{
    unsigned i;unsigned age=48-life;
    for(i=0;i<16;++i) {
        double angle=(double)i*2.*ARENA_PI/16.;double radius=3.+age*(.55+(i%4)*.15);
        int px=x+(int)lround(radius*sin(angle));
        int py=y+(int)lround(radius*cos(angle));
        dot(frame,px,py,age<12?1:(age<30?6:4));
    }
}
static void tracer(unsigned char *frame,const Bolt *b,const Scene *scene,unsigned char tip,
                   unsigned char trail)
{
    double ux,uy,x=b->x+.5*b->vx-(double)scene->camera_x;
    double y=b->y+.5*b->vy-(double)scene->camera_y;
    int k;
    if(!isfinite(x) || !isfinite(y) || x< -8. || x>808. || y<SKY_TOP-8. || y>SKY_BOTTOM+8.) return;
    motion_unit(b->angle,&ux,&uy);
    for(k=-3;k<=3;++k) dot(frame,(int)lround(x+k*ux),(int)lround(y+k*uy),k?trail:tip);
}
static void brackets(unsigned char *frame,int x,int y,unsigned char color)
{
    int side;
    for(side=-1;side<=1;side+=2) {
        int bx=x+side*43,by;
        for(by=-1;by<=1;by+=2) {
            int yy=y+by*39;
            line(frame,bx,yy,bx-side*11,yy,color);
            line(frame,bx,yy,bx,yy-by*11,color);
        }
    }
}
static double lesser(double a,double b) { return a<b?a:b; }
static void bearing(unsigned char *frame,double wx,double wy,const Scene *scene)
{
    double sx=wx-(double)scene->camera_x,sy=wy-(double)scene->camera_y;
    double dx=sx-400.,dy=sy-262.,scale=1e30;
    int x,y;
    if(!isfinite(dx) || !isfinite(dy) || (dx==0. && dy==0.)) return;
    if(dx>0.) scale=lesser(scale,386./dx);
    if(dx<0.) scale=lesser(scale,-386./dx);
    if(dy>0.) scale=lesser(scale,212./dy);
    if(dy<0.) scale=lesser(scale,-212./dy);
    if(!(scale>0.) || !isfinite(scale)) return;
    x=(int)lround(400.+dx*scale);y=(int)lround(262.+dy*scale);
    if(x<12) x=12;
    if(x>788) x=788;
    if(y<50) y=50;
    if(y>474) y=474;
    line(frame,x-6,y,x,y-6,6);line(frame,x,y-6,x+6,y,6);
    line(frame,x+6,y,x,y+6,6);line(frame,x,y+6,x-6,y,6);
    dot(frame,x,y,1);
}
static double fraction(double value,double maximum)
{
    double f=maximum>0. && isfinite(value)?value/maximum:0.;
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
static void radar(unsigned char *frame,const Arena *arena,const Pilot *pilot)
{
    const int x=646,y=505,w=140,h=73,cx=x+w/2,cy=y+h/2;
    unsigned i;
    box(frame,x,y,w,h,3);box(frame,x+1,y+1,w-2,h-2,10);
    line(frame,cx,y+4,cx,y+h-5,11);
    line(frame,x+4,cy,x+w-5,cy,11);
    /* The 140x73 scope shows a 2400x1200 world-unit neighborhood. */
    for(i=0;i<ARENA_ENEMIES;++i) if(!arena->enemies[i].destroyed) {
        double dx=arena->enemies[i].motion.x-pilot->state.x;
        double dy=arena->enemies[i].motion.y-pilot->state.y;
        double rx=dx/18.,ry=dy/18.;int px,py,clamped=0;
        if(!isfinite(rx) || !isfinite(ry)) continue;
        if(rx>64.) {rx=64.;clamped=1;}if(rx< -64.) {rx=-64.;clamped=1;}
        if(ry>31.) {ry=31.;clamped=1;}if(ry< -31.) {ry=-31.;clamped=1;}
        px=cx+(int)lround(rx);py=cy+(int)lround(ry);
        if((int)i==arena->selected) {
            line(frame,px-4,py,px+4,py,6);
            line(frame,px,py-4,px,py+4,6);
        } else box(frame,px-2,py-2,5,5,4);
        if(clamped) frame[py*VIDEO_WIDTH+px]=1;
    }
    box(frame,cx-2,cy-2,5,5,7);
    video_text(frame,x+4,y+5,"SCOPE",2);
}
static void hud(unsigned char *frame,const Arena *arena,const Practice *practice,
                const Pilot *pilot,const Threat *player)
{
    char label[128];const ArenaEnemy *target=NULL;
    unsigned alive=arena_alive(arena);
    box(frame,0,0,VIDEO_WIDTH,SKY_TOP,10);
    box(frame,0,SKY_BOTTOM,VIDEO_WIDTH,VIDEO_HEIGHT-SKY_BOTTOM,10);
    box(frame,0,SKY_TOP-1,VIDEO_WIDTH,1,7);
    box(frame,0,SKY_BOTTOM,VIDEO_WIDTH,1,7);
    video_text(frame,12,9,"ENDLESS SKY DOS  /  TRAINING GROUNDS",1);
    snprintf(label,sizeof(label),"BARGES %u/%u  %s",alive,ARENA_ENEMIES,
             player->destroyed?"PLAYER LOST":player->disabled?"HULL DISABLED":
             alive?"ENGAGED":"VICTORY");
    video_text(frame,530,9,label,player->destroyed||player->disabled?4:alive?6:5);
    video_text(frame,12,24,arena->stock?"STOCK HP":"TRAINING HP",6);
    video_text(frame,145,24,pilot->follow?"CAMERA FOLLOW":"CAMERA FIXED",2);
    snprintf(label,sizeof(label),"SPEED %.0f",hypot(pilot->state.vx,pilot->state.vy)*60.);
    video_text(frame,530,24,label,2);
    if(!practice->destructible)
        video_text(frame,320,24,"TARGETS INVULNERABLE",6);
    if(arena->selected>=0 && arena->selected<ARENA_ENEMIES)
        target=&arena->enemies[arena->selected];
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
    video_text(frame,323,495,target?arena->selected?"BARGE 2":"BARGE 1":"NO TARGET",6);
    if(target) {
        double dx=target->motion.x-pilot->state.x,dy=target->motion.y-pilot->state.y;
        double distance=hypot(dx,dy);
        snprintf(label,sizeof(label),"%s  RANGE %.0f",!practice->destructible?"INVULNERABLE":
                 target->destroyed?"DESTROYED":
                 target->power.disabled?"DISABLED":target->power.resources.overheated?"OVERHEATED":"ACTIVE",
                 isfinite(distance)?distance:0.);
        video_text(frame,323,510,label,target->destroyed||target->power.disabled?4:2);
        meter(frame,323,528,144,"SHIELD",target->health.shields,
              target->power.shield_profile.capacity,7);
        meter(frame,482,528,144,"HULL",target->health.hull,
              target->power.max_hull,5);
        snprintf(label,sizeof(label),"POWER %.0f%%  HEAT %.0f%%",
                 fraction(target->power.resources.energy,target->power.budget.capacity)*100.,
                 fraction(target->power.resources.heat,target->power.budget.max_heat)*100.);
        video_text(frame,323,562,label,2);
    } else video_text(frame,323,526,"T SELECTS  N NEAREST",2);
    radar(frame,arena,pilot);
    video_text(frame,12,588,"W THRUST  A/D TURN  SPACE FIRE  T/N TARGET  TAB CAMERA  R RESET  ESC EXIT",1);
}
void arena_view_draw(unsigned char *frame,const Arena *arena,const Practice *practice,
                     const Pilot *pilot,const Threat *player,const Scene *scene,
                     const Sprite sprites[6],const unsigned char *blend,
                     const unsigned char *add)
{
    unsigned i,j;
    backdrop(frame,scene);
    for(i=0;i<ARENA_ENEMIES;++i) {
        const ArenaEnemy *enemy=&arena->enemies[i];int x,y;
        if(screen_xy(enemy->motion.x,enemy->motion.y,scene,260,&x,&y)) {
            if(!enemy->destroyed)
                sprite_draw(frame,&sprites[1],
                    ((unsigned)enemy->motion.angle*sprites[1].frames+32768u)/65536u,
                    x,y,blend,add);
            if(enemy->explosion) spark(frame,x,y,enemy->explosion);
            if(!enemy->destroyed && arena->selected==(int)i &&
               x>=43 && x<757 && y>=SKY_TOP+39 && y<SKY_BOTTOM-39)
                brackets(frame,x,y,6);
        }
        if(!enemy->destroyed && arena->selected==(int)i &&
           (enemy->motion.x-(double)scene->camera_x<43. ||
            enemy->motion.x-(double)scene->camera_x>=757. ||
            enemy->motion.y-(double)scene->camera_y<SKY_TOP+39. ||
            enemy->motion.y-(double)scene->camera_y>=SKY_BOTTOM-39.))
            bearing(frame,enemy->motion.x,enemy->motion.y,scene);
    }
    for(i=0;i<PRACTICE_BOLTS;++i) if(practice->bolts[i].alive)
        tracer(frame,&practice->bolts[i],scene,1,7);
    for(i=0;i<ARENA_ENEMIES;++i)
        for(j=0;j<ARENA_BOLTS;++j) if(arena->enemies[i].bolts[j].alive)
            tracer(frame,&arena->enemies[i].bolts[j],scene,4,6);
    if(!player->destroyed) {
        int x,y;
        if(screen_xy(pilot->state.x,pilot->state.y,scene,100,&x,&y))
            sprite_draw(frame,&sprites[0],
                ((unsigned)pilot->state.angle*sprites[0].frames+32768u)/65536u,
                x,y,blend,add);
    }
    hud(frame,arena,practice,pilot,player);
}
