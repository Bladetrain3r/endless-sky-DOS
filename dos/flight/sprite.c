/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sprite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int compile_spans(Sprite *s)
{
    unsigned frame,y,x,n=s->width*s->height,pass;
    size_t count=0;
    /* Count first: no realloc churn, bounded by the already limited pixel size. */
    for(pass=0;pass<2;++pass) {
        count=0;
        for(frame=0;frame<s->frames;++frame) {
            const unsigned char *p=s->pixels+(size_t)frame*n*(s->mode?1:2);
            s->first_span[frame]=(uint32_t)count;
            for(y=0;y<s->height;++y) {
                x=0;
                while(x<s->width) {
                    unsigned at=y*s->width+x;
                    unsigned a=s->mode?(p[at]?1:0):p[n+at];
                    unsigned type=a?(s->mode?2:(a==15?0:1)):3;
                    unsigned start=x++;
                    while(x<s->width) {
                        unsigned next=y*s->width+x;
                        unsigned b=s->mode?(p[next]?1:0):p[n+next];
                        unsigned kind=b?(s->mode?2:(b==15?0:1)):3;
                        if(kind!=type) break;
                        ++x;
                    }
                    if(type==3) continue;
                    if(pass) {
                        SpriteSpan *span=&s->spans[count];
                        span->source_offset=y*s->width+start;
                        span->screen_offset=y*800+start;
                        span->x=(uint16_t)start; span->y=(uint16_t)y;
                        span->length=(uint16_t)(x-start); span->type=(uint8_t)type;
                    }
                    ++count;
                }
            }
        }
        s->first_span[s->frames]=(uint32_t)count;
        if(!pass) {
            if(count>262144 || !(s->spans=malloc((count?count:1)*sizeof(SpriteSpan)))) return 0;
            s->bytes+=(count?count:1)*sizeof(SpriteSpan);
        }
    }
    return 1;
}
static unsigned u16(const unsigned char *p) { return p[0]|(unsigned)p[1]<<8; }
void sprite_free(Sprite *s) { free(s->spans); free(s->pixels); memset(s,0,sizeof(*s)); }
int sprite_load(Sprite *s,const char *path)
{
    FILE *f=fopen(path,"rb"); unsigned char h[16];
    unsigned i,j,n; int ok=0;
    memset(s,0,sizeof(*s));
    if(!f) return 0;
    if(fread(h,1,16,f)!=16 || memcmp(h,"ESSPRT1\0",8)) goto done;
    s->width=u16(h+8); s->height=u16(h+10); s->frames=u16(h+12); s->mode=u16(h+14);
    if(!s->width || !s->height || s->width>512 || s->height>512 ||
       !s->frames || s->frames>16 || s->mode>1) goto done;
    n=s->width*s->height;
    s->bytes=(size_t)n*s->frames*(s->mode?1:2);
    if(s->bytes>2*1024*1024 || !(s->pixels=malloc(s->bytes))) goto done;
    for(i=0;i<s->frames;++i) {
        unsigned char *frame=s->pixels+(size_t)i*n*(s->mode?1:2);
        if(fread(frame,1,n,f)!=n) goto done;
        if(!s->mode) for(j=0;j<n;j+=2) {
            int v=fgetc(f); if(v==EOF) goto done;
            frame[n+j]=(unsigned char)(v&15);
            if(j+1<n) frame[n+j+1]=(unsigned char)(v>>4);
        }
    }
    if(fgetc(f)!=EOF || ferror(f)) goto done;
    if(!compile_spans(s)) goto done;
    ok=1;
done:
    fclose(f); if(!ok) sprite_free(s); return ok;
}
static void draw_run(unsigned char *dst,const unsigned char *src,unsigned length,
                     unsigned type,unsigned pixels,const unsigned char *blend,
                     const unsigned char *add)
{
    if(type==0) memcpy(dst,src,length);
    else if(type==2) {
        while(length--) { *dst=add[((unsigned)*src++<<8)|*dst]; ++dst; }
    } else {
        const unsigned char *alpha=src+pixels;
        while(length--) {
            *dst=blend[((unsigned)*alpha++<<16)|((unsigned)*src++<<8)|*dst];
            ++dst;
        }
    }
}
void sprite_draw(unsigned char *screen,const Sprite *s,unsigned heading,int cx,int cy,
                 const unsigned char *blend,const unsigned char *add)
{
    unsigned frame=heading%s->frames,n=s->width*s->height;
    uint32_t i;
    int x0=cx-(int)s->width/2,y0=cy-(int)s->height/2;
    const unsigned char *pixels=s->pixels+(size_t)frame*n*(s->mode?1:2);
    if(x0>=800 || y0>=600 || x0+(int)s->width<=0 || y0+(int)s->height<=0) return;
    if(x0>=0 && y0>=0 && x0+(int)s->width<=800 && y0+(int)s->height<=600) {
        unsigned char *origin=screen+(size_t)y0*800+x0;
        for(i=s->first_span[frame];i<s->first_span[frame+1];++i) {
            const SpriteSpan *span=&s->spans[i];
            draw_run(origin+span->screen_offset,pixels+span->source_offset,
                     span->length,span->type,n,blend,add);
        }
        return;
    }
    for(i=s->first_span[frame];i<s->first_span[frame+1];++i) {
        const SpriteSpan *span=&s->spans[i];
        int y=y0+span->y,x=x0+span->x,length=span->length,skip=0;
        if(y<0 || y>=600 || x>=800 || x+length<=0) continue;
        if(x<0) { skip=-x; length-=skip; x=0; }
        if(x+length>800) length=800-x;
        draw_run(screen+(size_t)y*800+x,pixels+span->source_offset+skip,
                 (unsigned)length,span->type,n,blend,add);
    }
}
