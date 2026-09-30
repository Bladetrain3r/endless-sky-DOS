/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "store.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Exhaust every byte natively; on the much slower emulator cover every header
 * byte plus regular payload offsets and the final byte, with all stage faults. */
static unsigned short_cases;
static int fault_offset(size_t i,size_t size)
{
#ifdef __DJGPP__
 return i<64 || i%32==0 || i+1==size;
#else
 (void)i;(void)size;return 1;
#endif
}
static void fixture(PSContext *c, PSState *s, unsigned pilot)
{
	unsigned i;
	memset(c, 0, sizeof(*c)); memset(s, 0, sizeof(*s));
	for(i = 0; i < 32; ++i) c->content[i] = (uint8_t)(i + 1);
	c->cargo_capacity = 15; c->max_crew = 6;
	c->max_fuel = 400.; c->max_shields = 200.; c->max_hull = 400.;
	for(i = 0; i < 16; ++i) {
		s->pilot_id[i] = (uint8_t)(pilot + i);
		s->ship_id[i] = s->flagship_id[i] = (uint8_t)(pilot + 32 + i);
	}
	strcpy(s->name, "Store Pilot"); strcpy(s->system, "system:Sol");
	strcpy(s->planet, "planet:Earth"); strcpy(s->model, "ship:Sparrow");
	strcpy(s->ship_name, "Needle");
	s->day = 16; s->month = 11; s->year = 3013; s->crew = 1;
	s->fuel = 400.; s->shields = 200.; s->hull = 400.; s->credits = 8485;
	s->cargo_count = 1; strcpy(s->cargo[0].key, "commodity:Food");
	s->cargo[0].tons = 3; s->cargo[0].basis = 1515;
}

static void path(char *out, size_t n, const char *dir, int slot)
{
	assert(snprintf(out, n, "%s/PILOT%d.SAV", dir, slot) > 0);
}

static void clear_dir(const char *dir)
{
	char p[512];
	path(p, sizeof(p), dir, 0); unlink(p);
	path(p, sizeof(p), dir, 1); unlink(p);
}

static size_t encode_file(const char *dir, int slot, const PSContext *c,
	const PSState *s, uint64_t generation)
{
	uint8_t data[PS_MAX_FILE]; size_t size = 0; char p[512]; FILE *f;
	assert(ps_encode(s, c, generation, data, sizeof(data), &size) == PS_OK);
	path(p, sizeof(p), dir, slot); f = fopen(p, "wb"); assert(f);
	assert(fwrite(data, 1, size, f) == size); assert(!fclose(f));
	return size;
}

static uint8_t *slurp(const char *dir, int slot, size_t *size)
{
	char p[512]; FILE *f; uint8_t *b;
	path(p, sizeof(p), dir, slot); f = fopen(p, "rb"); assert(f);
	assert(!fseek(f, 0, SEEK_END)); *size = (size_t)ftell(f); rewind(f);
	b = (uint8_t *)malloc(*size ? *size : 1); assert(b);
	assert(fread(b, 1, *size, f) == *size); assert(!fclose(f)); return b;
}

static void normal_and_faults(const char *dir, const PSContext *c, PSState *s)
{
	PSSession session, before; PSState out, sentinel; PSStoreFault fault;
	uint8_t *old; size_t old_size, encoded_size, i; char inactive[512];
	clear_dir(dir); ps_store_session_init(&session);
	assert(ps_store_load(dir, c, &out, &session) == PS_MISSING);
	assert(ps_store_save(dir, c, s, &session) == PS_OK);
	assert(session.generation == 1 && session.slot == 0);
	old = slurp(dir, 0, &old_size); encoded_size = old_size;
	s->credits++;
	path(inactive, sizeof(inactive), dir, 1);
	for(i = 0; i < encoded_size; ++i) {
        if(!fault_offset(i,encoded_size))continue;
        ++short_cases;
		unlink(inactive); before = session; fault.short_write_after = i;
		fault.stage = PS_FAULT_NONE;
		assert(ps_store_save_fault(dir, c, s, &session, &fault) == PS_IO);
		assert(!memcmp(&session, &before, sizeof(session)));
		{ size_t n; uint8_t *now = slurp(dir, 0, &n);
		  assert(n == old_size && !memcmp(now, old, n)); free(now); }
	}
	for(i = PS_FAULT_FLUSH; i <= PS_FAULT_READBACK; ++i) {
		unlink(inactive); before = session; fault.short_write_after = SIZE_MAX;
		fault.stage = (PSStoreFaultStage)i;
		assert(ps_store_save_fault(dir, c, s, &session, &fault) == PS_IO);
		assert(!memcmp(&session, &before, sizeof(session)));
        { size_t n; uint8_t *now=slurp(dir,0,&n);
          assert(n==old_size && !memcmp(now,old,n));free(now); }
    }
    /* A late injected failure may leave a valid newer candidate discoverable. */
	ps_store_session_init(&session); assert(ps_store_load(dir, c, &out, &session) == PS_OK);
	assert(out.credits == s->credits && session.generation == 2 && session.slot == 1);
	assert(ps_store_save(dir, c, s, &session) == PS_OK && session.generation == 3);
	ps_store_session_init(&session); memset(&out, 0, sizeof(out));
	assert(ps_store_load(dir, c, &out, &session) == PS_OK && out.credits == s->credits);
	/* Error publication is transactional. */
	sentinel = out; before = session; session.pilot_id[0] ^= 1;
	assert(ps_store_load(dir, c, &out, &session) == PS_CONFLICT);
	assert(!memcmp(&out, &sentinel, sizeof(out)));
	session = before; free(old);
}

static void first_save_and_recovery(const char *dir, const PSContext *c, PSState *s)
{
	PSSession session, before; PSStoreFault f; PSState out; size_t size, i; char p[512];
	clear_dir(dir); size = encode_file(dir, 0, c, s, 1); clear_dir(dir);
	for(i = 0; i < size; ++i) {
        if(!fault_offset(i,size))continue;
        ++short_cases;
		clear_dir(dir); ps_store_session_init(&session); before = session;
		f.short_write_after = i; f.stage = PS_FAULT_NONE;
		assert(ps_store_save_fault(dir, c, s, &session, &f) == PS_IO);
		assert(!memcmp(&session, &before, sizeof(session)));
		ps_store_session_init(&session);
		assert(ps_store_load(dir, c, &out, &session) == PS_CORRUPT);
	}
	clear_dir(dir); encode_file(dir, 0, c, s, 1); path(p, sizeof(p), dir, 1);
	{ FILE *x = fopen(p, "wb"); assert(x); fputc('x', x); fclose(x); }
	ps_store_session_init(&session); assert(ps_store_load(dir, c, &out, &session) == PS_OK);
	assert(session.recovered && session.slot == 0);
}

static void conflicts(const char *dir, PSContext *c, PSState *s)
{
	PSSession session, stale, before; PSState other, out, unchanged; PSContext foreign = *c;
	clear_dir(dir); encode_file(dir, 0, c, s, 4); other = *s; other.credits++;
	encode_file(dir, 1, c, &other, 4); ps_store_session_init(&session);
	assert(ps_store_load(dir, c, &out, &session) == PS_CONFLICT);
	clear_dir(dir); fixture(c, s, 1); fixture(&foreign, &other, 9);
	memcpy(foreign.content, c->content, sizeof(c->content));
	encode_file(dir, 0, c, s, 1); encode_file(dir, 1, c, &other, 2);
	assert(ps_store_load(dir, c, &out, &session) == PS_CONFLICT);
	clear_dir(dir); encode_file(dir, 0, c, s, 1); ps_store_session_init(&session);
	assert(ps_store_load(dir, c, &out, &session) == PS_OK); stale = session;
	encode_file(dir, 1, c, s, 2); unchanged = *s;
	assert(ps_store_save(dir, c, s, &stale) == PS_STALE);
	assert(!memcmp(s, &unchanged, sizeof(*s)));
	clear_dir(dir); encode_file(dir, 0, c, s, UINT64_MAX); ps_store_session_init(&session);
	assert(ps_store_load(dir, c, &out, &session) == PS_OK);
	assert(ps_store_save(dir, c, s, &session) == PS_EXHAUSTED);
	clear_dir(dir); encode_file(dir, 0, c, s, 1); foreign = *c; foreign.content[0] ^= 0xff;
	ps_store_session_init(&session); before = session;
	assert(ps_store_load(dir, &foreign, &out, &session) == PS_UNSUPPORTED);
	assert(ps_store_save(dir, &foreign, s, &session) == PS_UNSUPPORTED);
	assert(!memcmp(&session, &before, sizeof(session)));
	clear_dir(dir); ps_store_session_init(&session); session.pilot_id[0] = 99;
	assert(ps_store_save(dir, c, s, &session) == PS_CONFLICT);
}

int main(int argc, char **argv)
{
	PSContext context; PSState state; const char *dir;
	assert(argc == 1 || argc == 2); dir = argc == 2 ? argv[1] : ".";
	if(argc == 2 && mkdir(dir, 0700) && errno != EEXIST) return 2;
	fixture(&context, &state, 1); normal_and_faults(dir, &context, &state);
	fixture(&context, &state, 1); first_save_and_recovery(dir, &context, &state);
	fixture(&context, &state, 1); conflicts(dir, &context, &state);
	clear_dir(dir); printf("short_write_cases=%u\n",short_cases); puts("store tests passed"); return 0;
}
