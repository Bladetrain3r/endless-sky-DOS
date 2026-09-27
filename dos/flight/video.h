// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DOS_FLIGHT_VIDEO_H
#define DOS_FLIGHT_VIDEO_H

#define VIDEO_WIDTH 800
#define VIDEO_HEIGHT 600
#define VIDEO_PIXELS 480000

// Palette components are VGA DAC values (0..63), in RGB order.
int video_open(const unsigned char palette[768]);
int video_present(const unsigned char framebuffer[VIDEO_PIXELS]);
int video_verify(const unsigned char framebuffer[VIDEO_PIXELS]);
void video_close(void);
const char *video_error(void);
void video_text(unsigned char *frame, int x, int y, const char *s,
                unsigned char color);

#endif
