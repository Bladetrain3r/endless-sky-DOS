/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FLIGHT_INPUT_H
#define FLIGHT_INPUT_H
#include "input_state.h"

int input_open(void); /* 1 on success */
unsigned input_keys(void);
void input_close(void);
#endif
