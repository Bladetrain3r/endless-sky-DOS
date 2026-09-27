// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone headless DOSBox check of palette, bank upload, and readback.
#include "video.h"
#include <dpmi.h>
#include <stdio.h>
#include <string.h>

static unsigned char frame[VIDEO_PIXELS];
static unsigned char palette[768];

static unsigned long bios_ticks(void)
{
    __dpmi_regs r;
    memset(&r, 0, sizeof(r));
    r.h.ah = 0;
    if(__dpmi_int(0x1a, &r))
        return 0;
    return ((unsigned long)r.x.cx << 16) | r.x.dx;
}

int main(void)
{
    unsigned i, pass;
    unsigned long start, elapsed;
    for(i = 0; i < 256; ++i) {
        palette[i * 3] = i & 63;
        palette[i * 3 + 1] = (i * 5) & 63;
        palette[i * 3 + 2] = (i * 13) & 63;
    }
    if(!video_open(palette)) {
        printf("status=fail\nstage=open\nreason=%s\n", video_error());
        return 1;
    }
    for(pass = 0; pass < 3; ++pass) {
        for(i = 0; i < VIDEO_PIXELS; ++i)
            frame[i] = (unsigned char)((i / 65536u) * 29u +
                                       (i & 255u) * 3u + pass * 17u);
        video_text(frame, -2, 0, "Video 103h", 250);
        video_text(frame, 794, 596, "clip", 251);
        if(!video_present(frame) || !video_verify(frame)) {
            const char *reason = video_error();
            video_close();
            printf("status=fail\nstage=present_or_verify\nreason=%s\n",
                   reason);
            return 2;
        }
    }
    start = bios_ticks();
    for(i = 0; i < 120; ++i)
        if(!video_present(frame)) {
            const char *reason = video_error();
            video_close();
            printf("status=fail\nstage=timed_present\nreason=%s\n", reason);
            return 3;
        }
    elapsed = (bios_ticks() + 0x1800b0ul - start) % 0x1800b0ul;
    if(!video_verify(frame)) {
        const char *reason = video_error();
        video_close();
        printf("status=fail\nstage=post_timing_verify\nreason=%s\n", reason);
        return 4;
    }
    video_close();
    printf("probe=vbe_flight_video\ngeometry=800x600x8\n"
           "frames_checked=4\nbanks_checked=8\n"
           "palette_components_checked=768\n"
           "present_only_calls=120\npresent_only_bios_ticks=%lu\n"
           "status=pass\n", elapsed);
    return 0;
}
