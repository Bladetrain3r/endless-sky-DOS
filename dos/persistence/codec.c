/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "codec.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

#define HEADER 64u
typedef struct { uint8_t *p; size_t n, at; } Writer;
typedef struct { const uint8_t *p; size_t n, at; } Reader;

static void put(Writer *w, const void *p, size_t n)
{
 memcpy(w->p + w->at, p, n);
 w->at += n;
}

static void u16w(Writer *w, uint16_t v)
{
 uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
 put(w, b, 2);
}

static void u32w(Writer *w, uint32_t v)
{
 uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8),
  (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
 put(w, b, 4);
}

static void u64w(Writer *w, uint64_t v)
{
 u32w(w, (uint32_t)v);
 u32w(w, (uint32_t)(v >> 32));
}

static void strw(Writer *w, const char *s)
{
 size_t n = strlen(s);
 u16w(w, (uint16_t)n);
 put(w, s, n);
}

static uint32_t le32(const uint8_t *p)
{
 return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
  (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const uint8_t *p)
{
 return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}

static uint32_t crc_step(uint32_t c, const uint8_t *p, size_t n)
{
 size_t i;
 unsigned j;
 for(i = 0; i < n; ++i) {
  c ^= p[i];
  for(j = 0; j < 8; ++j)
   c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
 }
 return c;
}

static uint32_t file_crc(const uint8_t *p, size_t n)
{
 uint32_t c = crc_step(0xffffffffu, p, 60);
 return crc_step(c, p + HEADER, n - HEADER) ^ 0xffffffffu;
}

static size_t slen(const char *s) { return 2u + strlen(s); }

static void chunk(Writer *w, uint32_t type, uint32_t bytes)
{
 u32w(w, type);
 u32w(w, 1);
 u32w(w, bytes);
}
static int binary64_supported(void)
{
 return sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024;
}

PSResult ps_encode(const PSState *s, const PSContext *c, uint64_t gen,
 uint8_t *out, size_t cap, size_t *written)
{
 uint8_t *tmp;
 Writer w;
 size_t p1, p2, p3, p4, total;
 uint32_t i, crc;
 uint64_t bits;
 PSResult r;
 if(!s || !c || !out || !written || !gen)
  return PS_INVALID;
 if(!binary64_supported()) return PS_UNSUPPORTED;
 r = ps_validate(s, c);
 if(r != PS_OK) return r;
 p1 = 16 + slen(s->name) + 12 + slen(s->system) + slen(s->planet) + 16;
 p2 = 16 + slen(s->model) + slen(s->variant) + slen(s->ship_name) + 4 + 24;
 p3 = 12;
 for(i = 0; i < s->cargo_count; ++i) p3 += slen(s->cargo[i].key) + 12;
 p4 = 8;
 for(i = 0; i < s->sale_count; ++i)
  p4 += slen(s->sales[i].system) + slen(s->sales[i].commodity) + 4;
 total = HEADER + 48 + p1 + p2 + p3 + p4;
 if(total > PS_MAX_FILE || total > cap) return PS_INVALID;
 tmp = (uint8_t *)malloc(total);
 if(!tmp) return PS_MEMORY;
 memset(tmp, 0, HEADER);
 w.p = tmp; w.n = total; w.at = 0;

 put(&w, "ESDSAVE1", 8); u32w(&w, 1); u32w(&w, HEADER);
 u64w(&w, gen); u32w(&w, (uint32_t)(total - HEADER));
 put(&w, c->content, 32); u32w(&w, 0);
 chunk(&w, 1, (uint32_t)p1); put(&w, s->pilot_id, 16); strw(&w, s->name);
 u32w(&w, (uint32_t)s->day); u32w(&w, (uint32_t)s->month);
 u32w(&w, (uint32_t)s->year); strw(&w, s->system); strw(&w, s->planet);
 put(&w, s->flagship_id, 16);
 chunk(&w, 2, (uint32_t)p2); put(&w, s->ship_id, 16);
 strw(&w, s->model); strw(&w, s->variant); strw(&w, s->ship_name);
 u32w(&w, (uint32_t)s->crew);
 memcpy(&bits, &s->fuel, 8); u64w(&w, bits);
 memcpy(&bits, &s->shields, 8); u64w(&w, bits);
 memcpy(&bits, &s->hull, 8); u64w(&w, bits);
 chunk(&w, 3, (uint32_t)p3); u64w(&w, (uint64_t)s->credits);
 u32w(&w, s->cargo_count);
 for(i = 0; i < s->cargo_count; ++i) {
  strw(&w, s->cargo[i].key); u32w(&w, (uint32_t)s->cargo[i].tons);
  u64w(&w, (uint64_t)s->cargo[i].basis);
 }
 chunk(&w, 4, (uint32_t)p4); u32w(&w, s->scope_flags);
 u32w(&w, s->sale_count);
 for(i = 0; i < s->sale_count; ++i) {
  strw(&w, s->sales[i].system); strw(&w, s->sales[i].commodity);
  u32w(&w, (uint32_t)s->sales[i].delta);
 }
 crc = file_crc(tmp, total);
 tmp[60] = (uint8_t)crc; tmp[61] = (uint8_t)(crc >> 8);
 tmp[62] = (uint8_t)(crc >> 16); tmp[63] = (uint8_t)(crc >> 24);
 memcpy(out, tmp, total);
 *written = total;
 free(tmp);
 return PS_OK;
}

static int take(Reader *r, size_t n, const uint8_t **p)
{
 if(n > r->n - r->at) return 0;
 *p = r->p + r->at;
 r->at += n;
 return 1;
}

static int u32r(Reader *r, uint32_t *v)
{
 const uint8_t *p;
 if(!take(r, 4, &p)) return 0;
 *v = le32(p);
 return 1;
}

static int u64r(Reader *r, uint64_t *v)
{
 const uint8_t *p;
 if(!take(r, 8, &p)) return 0;
 *v = le64(p);
 return 1;
}

static int i32r(Reader *r, int32_t *v)
{
 uint32_t x;
 if(!u32r(r, &x)) return 0;
 *v = x <= INT32_MAX ? (int32_t)x :
  (int32_t)((int64_t)x - INT64_C(4294967296));
 return 1;
}

static int i64r(Reader *r, int64_t *v)
{
 uint64_t x;
 if(!u64r(r, &x)) return 0;
 *v = x <= INT64_MAX ? (int64_t)x :
  (int64_t)(x - ((uint64_t)INT64_MAX + 1u)) - INT64_MAX - 1;
 return 1;
}

static int strr(Reader *r, char out[PS_TEXT])
{
 const uint8_t *p;
 uint16_t n;
 if(r->n - r->at < 2) return 0;
 n = (uint16_t)r->p[r->at] | (uint16_t)r->p[r->at + 1] << 8;
 r->at += 2;
 if(n >= PS_TEXT || !take(r, n, &p) || memchr(p, 0, n)) return 0;
 memcpy(out, p, n);
 out[n] = 0;
 return 1;
}

static int f64r(Reader *r, double *v)
{
 uint64_t bits;
 if(!u64r(r, &bits)) return 0;
 memcpy(v, &bits, 8);
 return 1;
}

static PSResult start_chunk(Reader *all, Reader *chunk, uint32_t expected)
{
 uint32_t type, version, length;
 if(!u32r(all, &type) || !u32r(all, &version) || !u32r(all, &length) ||
  length > all->n - all->at) return PS_CORRUPT;
 if(type != expected || version != 1) return PS_UNSUPPORTED;
 chunk->p = all->p + all->at;
 chunk->n = length;
 chunk->at = 0;
 all->at += length;
 return PS_OK;
}

PSResult ps_decode(const uint8_t *d, size_t n, const PSContext *c,
 PSState *out, uint64_t *generation)
{
 PSState s;
 Reader all, q;
 uint32_t payload, count, i;
 uint64_t gen;
 const uint8_t *p;
 PSResult result;
 if(!d || !c || !out || !generation) return PS_INVALID;
 if(!binary64_supported()) return PS_UNSUPPORTED;
 if(n < 8 || memcmp(d, "ESDSAVE1", 8)) return PS_CORRUPT;
 if(n < 16) return PS_CORRUPT;
 /* Preserve recognizable future envelopes even when their checksum layout is
  * not yet known. Storage must never discard them as a torn current save. */
 if(le32(d + 8) != 1 || le32(d + 12) != HEADER) return PS_UNSUPPORTED;
 if(n < HEADER) return PS_CORRUPT;
 payload = le32(d + 24);
 if(payload > 65536u || (size_t)payload != n - HEADER ||
  le32(d + 60) != file_crc(d, n)) return PS_CORRUPT;
 if(memcmp(d + 28, c->content, 32)) return PS_UNSUPPORTED;
 gen = le64(d + 16);
 if(!gen) return PS_INVALID;
 memset(&s, 0, sizeof(s));
 all.p = d + HEADER; all.n = payload; all.at = 0;

 result = start_chunk(&all, &q, 1);
 if(result != PS_OK) return result;
 if(!take(&q, 16, &p)) return PS_CORRUPT;
 memcpy(s.pilot_id, p, 16);
 if(!strr(&q, s.name) || !i32r(&q, &s.day) || !i32r(&q, &s.month) ||
  !i32r(&q, &s.year) || !strr(&q, s.system) || !strr(&q, s.planet) ||
  !take(&q, 16, &p)) return PS_CORRUPT;
 memcpy(s.flagship_id, p, 16);
 if(q.at != q.n) return PS_CORRUPT;

 result = start_chunk(&all, &q, 2);
 if(result != PS_OK) return result;
 if(!take(&q, 16, &p)) return PS_CORRUPT;
 memcpy(s.ship_id, p, 16);
 if(!strr(&q, s.model) || !strr(&q, s.variant) || !strr(&q, s.ship_name) ||
  !i32r(&q, &s.crew) || !f64r(&q, &s.fuel) ||
  !f64r(&q, &s.shields) || !f64r(&q, &s.hull) || q.at != q.n)
  return PS_CORRUPT;

 result = start_chunk(&all, &q, 3);
 if(result != PS_OK) return result;
 if(!i64r(&q, &s.credits) || !u32r(&q, &count)) return PS_CORRUPT;
 if(count > PS_COMMODITIES) return PS_INVALID;
 s.cargo_count = count;
 for(i = 0; i < count; ++i)
  if(!strr(&q, s.cargo[i].key) || !i32r(&q, &s.cargo[i].tons) ||
   !i64r(&q, &s.cargo[i].basis)) return PS_CORRUPT;
 if(q.at != q.n) return PS_CORRUPT;

 result = start_chunk(&all, &q, 4);
 if(result != PS_OK) return result;
 if(!u32r(&q, &s.scope_flags) || !u32r(&q, &count)) return PS_CORRUPT;
 if(count > PS_SALES) return PS_INVALID;
 s.sale_count = count;
 for(i = 0; i < count; ++i)
  if(!strr(&q, s.sales[i].system) || !strr(&q, s.sales[i].commodity) ||
   !i32r(&q, &s.sales[i].delta)) return PS_CORRUPT;
 if(q.at != q.n) return PS_CORRUPT;
 if(all.at != all.n) return PS_UNSUPPORTED;

 result = ps_validate(&s, c);
 if(result != PS_OK) return result;
 *out = s;
 *generation = gen;
 return PS_OK;
}
