/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input_state.h"
#include <string.h>

void input_state_clear(InputState *state)
{
    memset(state,0,sizeof(*state));
}

/* Keyboard scan-code set 1. This function also runs inside IRQ1. */
__attribute__((noinline)) void input_state_feed(InputState *state, unsigned char code)
{
    unsigned char key;
    unsigned char was_down;
    if(state->pause_bytes) { --state->pause_bytes; return; }
    if(code==0xe1) { state->pause_bytes=5; state->prefix=0; return; }
    if(code==0xe0) { state->prefix=1; return; }
    key=code&0x7f;
    if(state->prefix) state->extended[key]=(code&0x80)?0:1;
    else {
        was_down=state->normal[key];
        state->normal[key]=(code&0x80)?0:1;
        if(!(code&0x80) && !was_down) {
            if(key==0x01) state->pending|=INPUT_EXIT;
            if(key==0x13) state->pending|=INPUT_RESET;
            if(key==0x0f) state->pending|=INPUT_CAMERA;
        }
    }
    state->prefix=0;
}

unsigned input_state_keys(const InputState *s)
{
    unsigned keys=0;
    if(s->normal[0x11] || s->extended[0x48] || s->normal[0x48]) keys|=INPUT_FORWARD;
    if(s->normal[0x1f] || s->extended[0x50] || s->normal[0x50]) keys|=INPUT_BACK;
    if(s->normal[0x1e] || s->extended[0x4b] || s->normal[0x4b]) keys|=INPUT_LEFT;
    if(s->normal[0x20] || s->extended[0x4d] || s->normal[0x4d]) keys|=INPUT_RIGHT;
    if(s->normal[0x01]) keys|=INPUT_EXIT;
    if(s->normal[0x13]) keys|=INPUT_RESET;
    if(s->normal[0x0f]) keys|=INPUT_CAMERA;
    return keys|s->pending;
}

unsigned input_state_take(InputState *state)
{
    unsigned keys=input_state_keys(state);
    state->pending=0;
    return keys;
}
