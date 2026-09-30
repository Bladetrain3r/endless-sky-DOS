/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ESDOS_PERSIST_CODEC_H
#define ESDOS_PERSIST_CODEC_H
#include "state.h"
/* Caller-owned bounded buffers; out/state/generation published only on success.
 * Integrity errors => CORRUPT. Intact unsupported schema/content/scope =>
 * UNSUPPORTED, so storage never silently overwrites that slot. */
PSResult ps_encode(const PSState *state,const PSContext *context,uint64_t generation,
                   uint8_t *out,size_t capacity,size_t *written);
PSResult ps_decode(const uint8_t *data,size_t size,const PSContext *context,
                   PSState *out,uint64_t *generation);
#endif
