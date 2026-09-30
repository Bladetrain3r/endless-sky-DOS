/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input_state.h"
#include <assert.h>
#include <stdio.h>

#ifndef __DJGPP__
int main(void)
{
    InputState s;
    input_state_clear(&s);
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0x11); input_state_feed(&s,0x39);
    assert(input_state_take(&s)==(INPUT_FORWARD|INPUT_FIRE));
    assert(input_state_take(&s)==(INPUT_FORWARD|INPUT_FIRE));
    input_state_feed(&s,0xb9);
    assert(input_state_take(&s)==INPUT_FORWARD);
    input_state_feed(&s,0x91);
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0x39); input_state_feed(&s,0xb9);
    assert(input_state_take(&s)==0); /* Fire release does not latch. */
    input_state_feed(&s,0x11); input_state_feed(&s,0x11);
    input_state_feed(&s,0xe0); input_state_feed(&s,0x48);
    input_state_feed(&s,0x91);
    assert(input_state_take(&s)==INPUT_FORWARD);
    input_state_feed(&s,0x1e); input_state_feed(&s,0x20);
    assert(input_state_take(&s)==(INPUT_FORWARD|INPUT_LEFT|INPUT_RIGHT));
    input_state_feed(&s,0xe0); input_state_feed(&s,0xc8);
    input_state_feed(&s,0x9e); input_state_feed(&s,0xa0);
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0x1f); input_state_feed(&s,0x9f);
    assert(input_state_take(&s)==0); /* Movement does not latch. */
    input_state_feed(&s,0x01); input_state_feed(&s,0x81);
    input_state_feed(&s,0x13); input_state_feed(&s,0x93);
    input_state_feed(&s,0x0f); input_state_feed(&s,0x8f);
    assert(input_state_take(&s)==(INPUT_EXIT|INPUT_RESET|INPUT_CAMERA));
    assert(input_state_take(&s)==0); /* Consumed exactly once. */
    input_state_feed(&s,0x13); input_state_feed(&s,0x13);
    assert(input_state_take(&s)==INPUT_RESET);
    assert(input_state_take(&s)==INPUT_RESET); /* Still physically held. */
    input_state_feed(&s,0x13); /* Auto-repeat while held must not relatch. */
    input_state_feed(&s,0x93);
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0x13); input_state_feed(&s,0x93);
    assert(input_state_take(&s)==INPUT_RESET); /* Release/repress latches. */
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0x23); input_state_feed(&s,0xa3);
    assert(input_state_take(&s)==INPUT_SHIELD_TEST); /* Quick trainer tap latches. */
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0x23); input_state_feed(&s,0x23);
    assert(input_state_take(&s)==INPUT_SHIELD_TEST);
    input_state_feed(&s,0x23); input_state_feed(&s,0xa3);
    assert(input_state_take(&s)==0); /* Holding H does not relatch. */
    input_state_feed(&s,0x14); input_state_feed(&s,0x14);
    assert(input_state_take(&s)==INPUT_TARGET_NEXT);
    input_state_feed(&s,0x14); input_state_feed(&s,0x94);
    assert(input_state_take(&s)==0); /* Repeat cannot relatch a target cycle. */
    input_state_feed(&s,0x14); input_state_feed(&s,0x94);
    input_state_feed(&s,0x31); input_state_feed(&s,0xb1);
    assert(input_state_take(&s)==(INPUT_TARGET_NEXT|INPUT_TARGET_NEAREST));
    assert(input_state_take(&s)==0);
    input_state_feed(&s,0xe1);
    input_state_feed(&s,0x1d); input_state_feed(&s,0x45);
    input_state_feed(&s,0xe1); input_state_feed(&s,0x9d); input_state_feed(&s,0xc5);
    input_state_feed(&s,0x7f); input_state_feed(&s,0xff);
    assert(input_state_take(&s)==0);
    puts("decoder=pass");
    return 0;
}
#else
#include "input.h"
#include <dos.h>
#include <dpmi.h>
#include <time.h>
int main(void)
{
    FILE *f;
    _go32_dpmi_seginfo before,after;
    unsigned long start=(unsigned long)clock();
    unsigned observed=0,changes=0,last=0;
    if(_go32_dpmi_get_protected_mode_interrupt_vector(9,&before)) return 2;
    if(!input_open()) { puts("open=failed"); return 2; }
    f=fopen("INPUT.TXT","w");
    if(!f) { input_close(); return 3; }
    fprintf(f,"open=ok\n"); fflush(f);
    while(((unsigned long)clock()-start)*1000UL/CLOCKS_PER_SEC<18000) {
        unsigned keys=input_keys();
        observed|=keys;
        if(keys!=last) {
            fprintf(f,"keys=%u\n",keys); fflush(f);
            last=keys; ++changes;
        }
        delay(5);
    }
    input_close();
    if(_go32_dpmi_get_protected_mode_interrupt_vector(9,&after) ||
       before.pm_offset!=after.pm_offset || before.pm_selector!=after.pm_selector) {
        fprintf(f,"closed=vector_mismatch\n"); fclose(f); return 4;
    }
    if(!input_open()) { fprintf(f,"reopen=failed\n"); fclose(f); return 5; }
    input_close();
    if(_go32_dpmi_get_protected_mode_interrupt_vector(9,&after) ||
       before.pm_offset!=after.pm_offset || before.pm_selector!=after.pm_selector) {
        fprintf(f,"closed=vector_mismatch_after_reopen\n"); fclose(f); return 6;
    }
    fprintf(f,"observed=%u\nchanges=%u\nreopen=ok\nclosed=ok\n",observed,changes);
    fclose(f);
    return 0;
}
#endif
