/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sprite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int read_file(const char *name,unsigned char *data,size_t n)
{
    FILE *f=fopen(name,"rb"); int ok;
    if(!f) return 0;
    ok=fread(data,1,n,f)==n && fgetc(f)==EOF;
    fclose(f); return ok;
}
int main(void)
{
    const char *names[]={"SPARROW.SPR","BARGE.SPR","FALCON.SPR","EARTH.SPR","LUNA.SPR","GLOW.SPR"};
    const int points[][2]={{400,300},{0,0},{799,599},{-300,300},{1100,300},{400,-300},{400,900},{10,300},{790,300}};
    unsigned char *a=malloc(480000),*b=malloc(480000),*blend=malloc(1048576),*add=malloc(65536);
    unsigned i,j,h,n,cases=0; Sprite s={0};
    if(!a||!b||!blend||!add||!read_file("BLEND.LUT",blend,1048576)||!read_file("ADD.LUT",add,65536)) return 1;
    for(i=0;i<6;++i) {
        if(!sprite_load(&s,names[i])) return 2;
        for(h=0;h<s.frames;++h) for(j=0;j<sizeof(points)/sizeof(points[0]);++j) {
            for(n=0;n<480000;++n) a[n]=(unsigned char)(n*31u+(n/800)*17u);
            memcpy(b,a,480000);
            sprite_draw(a,&s,h,points[j][0],points[j][1],blend,add);
            sprite_draw_reference(b,&s,h,points[j][0],points[j][1],blend,add);
            if(memcmp(a,b,480000)) { printf("mismatch=%s:%u:%u\n",names[i],h,j); return 3; }
            ++cases;
        }
        sprite_free(&s);
    }
    if(sprite_load(&s,"SHORT.SPR")) return 4;
    if(sprite_load(&s,"HUGE.SPR")) return 5;
    if(sprite_load(&s,"FRAMES.SPR")) return 6;
    printf("status=pass\nreference_pixel_cases=%u\nmalformed_sprites_rejected=3\n",cases);
    free(a); free(b); free(blend); free(add); return 0;
}
