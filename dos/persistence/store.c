/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __DJGPP__
#define _POSIX_C_SOURCE 200809L
#endif
#include "store.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __DJGPP__
#include <dpmi.h>
#endif

typedef struct {
	PSResult result;
	uint8_t *bytes;
	size_t size;
	PSState state;
	uint64_t generation;
	uint32_t token;
} Slot;

static int id_zero(const uint8_t id[16])
{
	size_t i;
	for(i = 0; i < 16; ++i)
		if(id[i]) return 0;
	return 1;
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static PSResult make_path(char out[PS_STORE_PATH], const char *directory, int slot)
{
	int n;
	if(!directory || !*directory || (slot != 0 && slot != 1)) return PS_INVALID;
	n = snprintf(out, PS_STORE_PATH, "%s%sPILOT%d.SAV", directory,
		directory[strlen(directory) - 1] == '/' ? "" : "/", slot);
	return n < 0 || n >= PS_STORE_PATH ? PS_INVALID : PS_OK;
}

static void slot_clear(Slot *slot)
{
	free(slot->bytes);
	memset(slot, 0, sizeof(*slot));
}

static PSResult read_slot(const char *directory, int index,
	const PSContext *context, Slot *slot)
{
	char path[PS_STORE_PATH];
	FILE *file;
	size_t got;
	PSResult result = make_path(path, directory, index);
	memset(slot, 0, sizeof(*slot));
	if(result != PS_OK) return slot->result = result;
	file = fopen(path, "rb");
	if(!file) return slot->result = (errno == ENOENT ? PS_MISSING : PS_IO);
	slot->bytes = (uint8_t *)malloc(PS_MAX_FILE + 1u);
	if(!slot->bytes) { fclose(file); return slot->result = PS_MEMORY; }
	got = fread(slot->bytes, 1, PS_MAX_FILE + 1u, file);
	{
		int read_error = ferror(file);
		int close_error = fclose(file) != 0;
		if(read_error || close_error) return slot->result = PS_IO;
	}
	slot->size = got;
	if(got > PS_MAX_FILE) return slot->result = PS_CORRUPT;
	result = ps_decode(slot->bytes, got, context, &slot->state, &slot->generation);
	if(result == PS_OK) slot->token = le32(slot->bytes + 60);
	return slot->result = result;
}

static PSResult inspect(const char *directory, const PSContext *context,
	Slot slots[2], int *selected, int *recovered)
{
	struct stat info;
	if(!directory || stat(directory, &info) || !S_ISDIR(info.st_mode)) {
		memset(slots, 0, sizeof(Slot) * 2u);
		return PS_IO;
	}
	PSResult a = read_slot(directory, 0, context, &slots[0]);
	PSResult b = read_slot(directory, 1, context, &slots[1]);
	int good0 = a == PS_OK, good1 = b == PS_OK;
	*selected = -1;
	*recovered = 0;
	if(a == PS_UNSUPPORTED || b == PS_UNSUPPORTED) return PS_UNSUPPORTED;
	if(a == PS_IO || b == PS_IO || a == PS_MEMORY || b == PS_MEMORY ||
		a == PS_INVALID || b == PS_INVALID) return a != PS_OK && a != PS_MISSING &&
		 a != PS_CORRUPT ? a : b;
	if(!good0 && !good1) {
		if(a == PS_MISSING && b == PS_MISSING) return PS_MISSING;
		return PS_CORRUPT;
	}
	if(good0 && good1) {
		if(memcmp(slots[0].state.pilot_id, slots[1].state.pilot_id, 16))
			return PS_CONFLICT;
		if(slots[0].generation == slots[1].generation) {
			if(slots[0].size != slots[1].size ||
				memcmp(slots[0].bytes, slots[1].bytes, slots[0].size))
				return PS_CONFLICT;
			*selected = 0;
		} else *selected = slots[1].generation > slots[0].generation ? 1 : 0;
		return PS_OK;
	}
	*selected = good0 ? 0 : 1;
	*recovered = ((good0 ? b : a) == PS_CORRUPT);
	return PS_OK;
}

void ps_store_session_init(PSSession *session)
{
	if(session) {
		memset(session, 0, sizeof(*session));
		session->slot = -1;
	}
}

PSResult ps_store_load(const char *directory, const PSContext *context,
	PSState *state, PSSession *session)
{
	Slot slots[2];
	PSSession next;
	PSState candidate;
	PSResult result;
	int selected, recovered;
	uint8_t expected[16];
	if(!context || !state || !session) return PS_INVALID;
	memcpy(expected, session->pilot_id, 16);
	result = inspect(directory, context, slots, &selected, &recovered);
	if(result == PS_OK) {
		candidate = slots[selected].state;
		if(!id_zero(expected) && memcmp(expected, candidate.pilot_id, 16))
			result = PS_CONFLICT;
		else {
			next.generation = slots[selected].generation;
			next.slot = selected;
			memcpy(next.pilot_id, candidate.pilot_id, 16);
			next.crc_token = slots[selected].token;
			next.recovered = recovered;
			*state = candidate;
			*session = next;
		}
	}
	slot_clear(&slots[0]); slot_clear(&slots[1]);
	return result;
}

static int session_matches(const PSSession *session, const Slot *slot, int selected)
{
	if(selected < 0)
		return session->generation == 0 && session->slot == -1 &&
			session->crc_token == 0;
	return session->generation == slot->generation && session->slot == selected &&
		session->crc_token == slot->token &&
		!memcmp(session->pilot_id, slot->state.pilot_id, 16);
}

static int commit_file(FILE *file)
{
	int fd = fileno(file);
#ifdef __DJGPP__
	__dpmi_regs regs;
	memset(&regs, 0, sizeof(regs));
	regs.x.ax = 0x6800;
	regs.x.bx = (unsigned short)fd;
	/* Unlike DJGPP fsync(), reject every DOS carry result, including AX=1/6. */
	return fd < 0 || __dpmi_int(0x21, &regs) != 0 || (regs.x.flags & 1u);
#else
	return fd < 0 || fsync(fd) != 0;
#endif
}

static PSResult write_candidate(const char *directory, int target,
	const uint8_t *bytes, size_t size, const PSStoreFault *fault)
{
	char path[PS_STORE_PATH];
	FILE *file;
	size_t limit = fault ? fault->short_write_after : SIZE_MAX;
	size_t wanted = limit < size ? limit : size;
	PSResult result = make_path(path, directory, target);
	if(result != PS_OK) return result;
	file = fopen(path, "wb");
	if(!file) return PS_IO;
	if(wanted && fwrite(bytes, 1, wanted, file) != wanted) result = PS_IO;
	if(result == PS_OK && wanted != size) result = PS_IO;
	if(result == PS_OK && fault && fault->stage == PS_FAULT_FLUSH) result = PS_IO;
	if(result == PS_OK && fflush(file) != 0) result = PS_IO;
	if(result == PS_OK && fault && fault->stage == PS_FAULT_COMMIT) result = PS_IO;
	/* DOS uses INT 21h/68h directly; native hosts use fsync(). */
	if(result == PS_OK && commit_file(file)) result = PS_IO;
	if(fclose(file) != 0) result = PS_IO;
	if(result == PS_OK && fault && fault->stage == PS_FAULT_CLOSE) result = PS_IO;
	if(result == PS_OK && fault && fault->stage == PS_FAULT_READBACK) result = PS_IO;
	return result;
}

PSResult ps_store_save_fault(const char *directory, const PSContext *context,
	const PSState *state, PSSession *session, const PSStoreFault *fault)
{
	Slot slots[2], check;
	PSSession next;
	uint8_t *encoded = NULL;
	size_t size = 0;
	PSResult result;
	int selected, recovered, target;
	uint64_t generation;
	if(!context || !state || !session) return PS_INVALID;
	if(!id_zero(session->pilot_id) &&
		memcmp(session->pilot_id, state->pilot_id, 16)) return PS_CONFLICT;
	result = inspect(directory, context, slots, &selected, &recovered);
	if(result == PS_MISSING) selected = -1;
	else if(result != PS_OK) goto done;
	if(!session_matches(session, selected < 0 ? NULL : &slots[selected], selected)) {
		result = PS_STALE; goto done;
	}
	if(selected >= 0 && memcmp(state->pilot_id, slots[selected].state.pilot_id, 16)) {
		result = PS_CONFLICT; goto done;
	}
	if(selected >= 0 && slots[selected].generation == UINT64_MAX) {
		result = PS_EXHAUSTED; goto done;
	}
	generation = selected < 0 ? 1u : slots[selected].generation + 1u;
	target = selected < 0 ? 0 : 1 - selected;
	encoded = (uint8_t *)malloc(PS_MAX_FILE);
	if(!encoded) { result = PS_MEMORY; goto done; }
	result = ps_encode(state, context, generation, encoded, PS_MAX_FILE, &size);
	if(result != PS_OK) goto done;
	result = write_candidate(directory, target, encoded, size, fault);
	if(result != PS_OK) goto done;
	result = read_slot(directory, target, context, &check);
	if(result != PS_OK) { slot_clear(&check); goto done; }
	if(check.generation != generation || check.size != size ||
		memcmp(check.bytes, encoded, size)) result = PS_IO;
	else {
		next.generation = generation;
		next.slot = target;
		memcpy(next.pilot_id, state->pilot_id, 16);
		next.crc_token = check.token;
		next.recovered = 0;
		*session = next;
	}
	slot_clear(&check);
done:
	free(encoded);
	slot_clear(&slots[0]); slot_clear(&slots[1]);
	return result;
}

PSResult ps_store_save(const char *directory, const PSContext *context,
	const PSState *state, PSSession *session)
{
	PSStoreFault fault;
	fault.short_write_after = SIZE_MAX;
	fault.stage = PS_FAULT_NONE;
	return ps_store_save_fault(directory, context, state, session, &fault);
}
