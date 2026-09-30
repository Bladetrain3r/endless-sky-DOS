/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_INPUT_STATE_H
#define FLIGHT_INPUT_STATE_H

#define INPUT_FORWARD 1u
#define INPUT_BACK 2u
#define INPUT_LEFT 4u
#define INPUT_RIGHT 8u
#define INPUT_EXIT 16u
#define INPUT_RESET 32u
#define INPUT_CAMERA 64u
#define INPUT_FIRE 128u
#define INPUT_SHIELD_TEST 256u
#define INPUT_TARGET_NEXT 512u
#define INPUT_TARGET_NEAREST 1024u
#define INPUT_LAND 2048u
#define INPUT_PLANET 4096u

typedef struct {
    volatile unsigned char normal[128];
    volatile unsigned char extended[128];
    volatile unsigned char prefix;
    volatile unsigned char pause_bytes;
    volatile unsigned pending;
} InputState;

void input_state_clear(InputState *state);
void input_state_feed(InputState *state, unsigned char scancode);
unsigned input_state_keys(const InputState *state);
unsigned input_state_take(InputState *state);

#endif
