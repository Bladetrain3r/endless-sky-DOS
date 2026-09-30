/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ESDOS_PERSIST_STORE_H
#define ESDOS_PERSIST_STORE_H
#include "codec.h"

#define PS_STORE_PATH 512
/* Conservative transient heap bounds, excluding caller-owned state/context. */
#define PS_STORE_LOAD_HEAP_MAX (2u * (PS_MAX_FILE + 1u))
#define PS_STORE_SAVE_HEAP_MAX (4u * (PS_MAX_FILE + 1u))

typedef struct {
	uint64_t generation;
	int slot;                 /* -1 before the first save, otherwise 0 or 1. */
	uint8_t pilot_id[16];     /* Nonzero also acts as an expected identity on load. */
	uint32_t crc_token;       /* Header CRC of the acknowledged generation. */
	int recovered;            /* Newest physical candidate was corrupt/incomplete. */
} PSSession;

typedef enum {
	PS_FAULT_NONE = 0,
	PS_FAULT_FLUSH,
	PS_FAULT_COMMIT,
	PS_FAULT_CLOSE,
	PS_FAULT_READBACK
} PSStoreFaultStage;

typedef struct {
	size_t short_write_after; /* SIZE_MAX or >= encoded size disables; lower injects. */
	PSStoreFaultStage stage;
} PSStoreFault;

void ps_store_session_init(PSSession *session);
PSResult ps_store_load(const char *directory, const PSContext *context,
	PSState *state, PSSession *session);
PSResult ps_store_save(const char *directory, const PSContext *context,
	const PSState *state, PSSession *session);
/* Deterministic test seam; production callers use ps_store_save(). */
PSResult ps_store_save_fault(const char *directory, const PSContext *context,
	const PSState *state, PSSession *session, const PSStoreFault *fault);

#endif
