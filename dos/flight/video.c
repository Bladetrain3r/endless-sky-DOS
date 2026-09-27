// SPDX-License-Identifier: GPL-3.0-or-later
#include "video.h"
#include <dpmi.h>
#include <pc.h>
#include <sys/movedata.h>
#include <string.h>

static int opened;
static unsigned bank_step;
static unsigned char dac[768];
static const char *error_message = "closed";

static unsigned short word_at(const unsigned char *p)
{
    return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
}

static int bios(unsigned short ax, unsigned short bx, unsigned short cx,
                unsigned short dx, unsigned short es, unsigned short di)
{
    __dpmi_regs r;
    memset(&r, 0, sizeof(r));
    r.x.ax = ax;
    r.x.bx = bx;
    r.x.cx = cx;
    r.x.dx = dx;
    r.x.es = es;
    r.x.di = di;
    if(__dpmi_int(0x10, &r))
        return -1;
    return r.x.ax;
}

static int select_bank(unsigned bank)
{
    if(bios(0x4f05, 0, 0, (unsigned short)(bank * bank_step), 0, 0) != 0x004f) {
        error_message = "VBE bank switch failed";
        return 0;
    }
    return 1;
}

int video_open(const unsigned char palette[768])
{
    int selector = 0;
    int segment;
    unsigned char info[256];
    unsigned i;
    if(opened) {
        error_message = "video already open";
        return 0;
    }
    if(!palette) {
        error_message = "null palette";
        return 0;
    }
    for(i = 0; i < 768; ++i)
        if(palette[i] > 63) {
            error_message = "palette component exceeds six bits";
            return 0;
        }
    segment = __dpmi_allocate_dos_memory(16, &selector);
    if(segment < 0) {
        error_message = "DOS mode-info buffer allocation failed";
        return 0;
    }
    // VBE 4F01h writes its 256-byte ModeInfoBlock into conventional memory.
    if(bios(0x4f01, 0, 0x103, 0, (unsigned short)segment, 0) != 0x004f) {
        error_message = "VBE mode 103h query failed";
        __dpmi_free_dos_memory(selector);
        return 0;
    }
    dosmemget((unsigned long)segment << 4, sizeof(info), info);
    __dpmi_free_dos_memory(selector);
    if(!(word_at(info) & 1) || (word_at(info) & 32) || (info[2] & 5) != 5 ||
       word_at(info + 8) != 0xa000 || word_at(info + 6) < 64 ||
       !word_at(info + 4) || 64 % word_at(info + 4) ||
       word_at(info + 16) != 800 || word_at(info + 18) != 800 ||
       word_at(info + 20) != 600 || info[24] != 1 || info[25] != 8 ||
       info[27] != 4) {
        error_message = "unsupported VBE 800x600x8 bank layout";
        return 0;
    }
    bank_step = 64 / word_at(info + 4);
    if(bios(0x4f02, 0x103, 0, 0, 0, 0) != 0x004f) {
        error_message = "VBE mode set failed";
        return 0;
    }
    opened = 1;
    memcpy(dac, palette, sizeof(dac));
    outportb(0x3c8, 0);
    for(i = 0; i < 768; ++i)
        outportb(0x3c9, dac[i]);
    error_message = "ok";
    return 1;
}

int video_present(const unsigned char framebuffer[VIDEO_PIXELS])
{
    unsigned bank;
    if(!opened || !framebuffer) {
        error_message = "present requires open video and a framebuffer";
        return 0;
    }
    for(bank = 0; bank < 8; ++bank) {
        const unsigned offset = bank * 65536u;
        const unsigned count = bank == 7 ? VIDEO_PIXELS - offset : 65536u;
        if(!select_bank(bank))
            return 0;
        dosmemput(framebuffer + offset, count, 0xa0000ul);
    }
    error_message = "ok";
    return 1;
}

int video_verify(const unsigned char framebuffer[VIDEO_PIXELS])
{
    unsigned bank, i;
    if(!opened || !framebuffer) {
        error_message = "verify requires open video and a framebuffer";
        return 0;
    }
    for(bank = 0; bank < 8; ++bank) {
        const unsigned base = bank * 65536u;
        const unsigned count = bank == 7 ? VIDEO_PIXELS - base : 65536u;
        const unsigned samples[5] = {0, 1, count / 4, count / 2, count - 1};
        if(!select_bank(bank))
            return 0;
        for(i = 0; i < 5; ++i) {
            unsigned char actual;
            dosmemget(0xa0000ul + samples[i], 1, &actual);
            if(actual != framebuffer[base + samples[i]]) {
                error_message = "VBE framebuffer readback mismatch";
                return 0;
            }
        }
    }
    outportb(0x3c7, 0);
    for(i = 0; i < 768; ++i)
        if(inportb(0x3c9) != dac[i]) {
            error_message = "VGA DAC readback mismatch";
            return 0;
        }
    error_message = "ok";
    return 1;
}

void video_close(void)
{
    if(opened) {
        bios(3, 0, 0, 0, 0, 0);
        opened = 0;
    }
    error_message = "closed";
}

const char *video_error(void)
{
    return error_message;
}

// Original compact 5x7 block font. Rows use bits 4..0, left to right.
typedef struct { char letter; unsigned char rows[7]; } Glyph;
static const Glyph glyphs[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'.',{0,0,0,0,0,12,12}}, {':',{0,12,12,0,12,12,0}},
    {'-',{0,0,0,31,0,0,0}}, {'/',{1,1,2,4,8,16,16}},
    {'+',{0,4,4,31,4,4,0}}, {'=',{0,0,31,0,31,0,0}},
    {'!',{4,4,4,4,4,0,4}}, {'?',{14,17,1,2,4,0,4}},
    {'%',{17,1,2,4,8,16,17}}, {'(',{2,4,8,8,8,4,2}},
    {')',{8,4,2,2,2,4,8}}, {'_',{0,0,0,0,0,0,31}},
    {'#',{10,10,31,10,31,10,10}}, {',',{0,0,0,0,12,12,8}}
};

void video_text(unsigned char *frame, int x, int y, const char *s,
                unsigned char color)
{
    if(!frame || !s || y < -6 || y >= VIDEO_HEIGHT)
        return;
    while(*s) {
        unsigned char ch = (unsigned char)*s++;
        unsigned g, row, col;
        if(x >= VIDEO_WIDTH)
            break;
        if(ch >= 'a' && ch <= 'z')
            ch = (unsigned char)(ch - 'a' + 'A');
        for(g = 0; g < sizeof(glyphs) / sizeof(glyphs[0]); ++g)
            if((unsigned char)glyphs[g].letter == ch)
                break;
        if(g < sizeof(glyphs) / sizeof(glyphs[0]))
            for(row = 0; row < 7; ++row)
                for(col = 0; col < 5; ++col)
                    if((glyphs[g].rows[row] & (16u >> col)) &&
                       x + (int)col >= 0 && x + (int)col < VIDEO_WIDTH &&
                       y + (int)row >= 0 && y + (int)row < VIDEO_HEIGHT)
                        frame[(y + (int)row) * VIDEO_WIDTH + x + (int)col] = color;
        x += 6;
    }
}
