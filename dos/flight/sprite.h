/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_SPRITE_H
#define FLIGHT_SPRITE_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t source_offset,screen_offset;
    uint16_t x,y,length;
    uint8_t type;
} SpriteSpan;
typedef struct {
    unsigned width,height,frames,mode;
    unsigned char *pixels;
    size_t bytes;
    SpriteSpan *spans;
    uint32_t first_span[17];
} Sprite;
int sprite_load(Sprite *s,const char *path);
void sprite_free(Sprite *s);
void sprite_draw(unsigned char *screen,const Sprite *s,unsigned heading,int cx,int cy,
                 const unsigned char *blend,const unsigned char *add);
void sprite_draw_reference(unsigned char *screen,const Sprite *s,unsigned heading,int cx,int cy,
                           const unsigned char *blend,const unsigned char *add);
#endif
