/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input.h"
#include <dpmi.h>
#include <go32.h>
#include <dos.h>
#include <string.h>

static InputState state;
static _go32_dpmi_seginfo old_irq,our_irq;
static int installed,wrapped;

/* Inline I/O keeps the IRQ path independent of pageable C library routines. */
static __attribute__((noinline)) void keyboard_irq(void)
{
    unsigned char code;
    __asm__ volatile("inb $0x60,%0" : "=a"(code));
    input_state_feed(&state,code);
    __asm__ volatile("outb %%al,$0x20" :: "a"((unsigned char)0x20));
}

int input_open(void)
{
    if(installed) return 1;
    input_state_clear(&state);
    memset(&our_irq,0,sizeof(our_irq));
    if(_go32_dpmi_lock_data(&state,sizeof(state)) ||
       _go32_dpmi_lock_code((void *)keyboard_irq,4096) ||
       _go32_dpmi_lock_code((void *)input_state_feed,4096)) return 0;
    if(_go32_dpmi_get_protected_mode_interrupt_vector(9,&old_irq)) return 0;
    our_irq.pm_offset=(unsigned long)keyboard_irq;
    our_irq.pm_selector=_go32_my_cs();
    if(_go32_dpmi_allocate_iret_wrapper(&our_irq)) return 0;
    wrapped=1;
    disable();
    if(_go32_dpmi_set_protected_mode_interrupt_vector(9,&our_irq)) {
        enable();
        _go32_dpmi_free_iret_wrapper(&our_irq);
        wrapped=0;
        return 0;
    }
    installed=1;
    enable();
    return 1;
}

unsigned input_keys(void)
{
    unsigned keys;
    if(!installed) return 0;
    disable();
    keys=input_state_take(&state);
    enable();
    return keys;
}

void input_close(void)
{
    if(!installed) return;
    disable();
    if(_go32_dpmi_set_protected_mode_interrupt_vector(9,&old_irq)) {
        enable();
        return; /* Keep the live wrapper allocated if restoration failed. */
    }
    installed=0;
    enable();
    if(wrapped) { _go32_dpmi_free_iret_wrapper(&our_irq); wrapped=0; }
    input_state_clear(&state);
}
