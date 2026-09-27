/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Scalar correctness/performance reference; deliberately unoptimized. */
#include "sprite.h"
#include <stddef.h>
void sprite_draw_reference(unsigned char *screen,const Sprite *s,unsigned heading,int cx,int cy,
                 const unsigned char *blend,const unsigned char *add)
{
    int x0=cx-(int)s->width/2,y0=cy-(int)s->height/2;
    int left=x0<0?-x0:0,top=y0<0?-y0:0;
    int right=x0+(int)s->width>800?800-x0:(int)s->width;
    int bottom=y0+(int)s->height>600?600-y0:(int)s->height;
    int x,y; unsigned n=s->width*s->height;
    const unsigned char *pixels=s->pixels+(size_t)(heading%s->frames)*n*(s->mode?1:2);
    if(left>=right || top>=bottom) return;
    for(y=top;y<bottom;++y) {
        const unsigned char *src=pixels+(size_t)y*s->width;
        unsigned char *dst=screen+(size_t)(y0+y)*800+x0+left;
        const unsigned char *alpha=s->mode?NULL:src+n;
        for(x=left;x<right;++x,++dst) {
            unsigned c=src[x];
            if(s->mode) { if(c) *dst=add[(c<<8)|*dst]; }
            else {
                unsigned a=alpha[x];
                if(a==15) *dst=(unsigned char)c;
                else if(a) *dst=blend[(a<<16)|(c<<8)|*dst];
            }
        }
    }
}
