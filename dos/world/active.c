/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "active.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BYTES 24u
#define ENTRY_BYTES 24u
#define MAX_RECORDS 8192u
#define MAX_RECORD_BYTES 32768u
#define STORE_BUDGET 262144u
#define ACTIVE_BUDGET 65536u

struct WorldStore {
    FILE *file;
    unsigned char *directory;
    unsigned char *scratch;
    uint32_t count;
    uint32_t max_record;
    size_t bytes;
    const char *error;
};

typedef struct {
    const unsigned char *data;
    uint32_t size;
    uint32_t at;
} Cursor;

typedef struct {
    unsigned char *base;
    size_t used;
} Arena;

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const unsigned char *p)
{
    return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}

static uint32_t fnv(const unsigned char *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    uint32_t i;
    for(i = 0; i < n; ++i)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static int take(Cursor *c, uint32_t n, const unsigned char **p)
{
    if(n > c->size - c->at)
        return 0;
    *p = c->data + c->at;
    c->at += n;
    return 1;
}

static int u32(Cursor *c, uint32_t *v)
{
    const unsigned char *p;
    if(!take(c, 4, &p))
        return 0;
    *v = le32(p);
    return 1;
}

static int i32(Cursor *c, int32_t *v)
{
    uint32_t n;
    if(!u32(c, &n))
        return 0;
    *v = n <= INT32_MAX ? (int32_t)n :
         (int32_t)((int64_t)n - INT64_C(4294967296));
    return 1;
}

static int u64(Cursor *c, uint64_t *v)
{
    const unsigned char *p;
    if(!take(c, 8, &p))
        return 0;
    *v = le64(p);
    return 1;
}

static int f64(Cursor *c, double *v)
{
    uint64_t bits;
    if(!u64(c, &bits) ||
       (bits & UINT64_C(0x7ff0000000000000)) ==
       UINT64_C(0x7ff0000000000000))
        return 0;
    if(v)
        memcpy(v, &bits, 8);
    return 1;
}

/* Reject embedded NUL, overlong encodings, surrogates and non-scalar values. */
static int utf8(const unsigned char *p, uint32_t n)
{
    uint32_t i = 0;
    while(i < n) {
        uint32_t cp, low, need, j;
        unsigned char b = p[i++];
        if(!b)
            return 0;
        if(b < 128)
            continue;
        if(b >= 0xc2 && b <= 0xdf) {
            cp = b & 31;
            low = 128;
            need = 1;
        } else if(b >= 0xe0 && b <= 0xef) {
            cp = b & 15;
            low = 2048;
            need = 2;
        } else if(b >= 0xf0 && b <= 0xf4) {
            cp = b & 7;
            low = 65536;
            need = 3;
        } else {
            return 0;
        }
        if(need > n - i)
            return 0;
        for(j = 0; j < need; ++j) {
            b = p[i++];
            if((b & 0xc0) != 0x80)
                return 0;
            cp = (cp << 6) | (b & 63);
        }
        if(cp < low || cp > 0x10ffff ||
           (cp >= 0xd800 && cp <= 0xdfff))
            return 0;
    }
    return 1;
}

/* With base NULL, reserve only counts bytes for the first decode pass. */
static void *reserve(Arena *a, size_t n, size_t align)
{
    size_t at = (a->used + align - 1) & ~(align - 1);
    if(at > ACTIVE_BUDGET || n > ACTIVE_BUDGET - at)
        return NULL;
    a->used = at + n;
    return a->base ? a->base + at : (void *)a;
}

static int str(Cursor *c, int nullable, Arena *a, const char **out)
{
    uint32_t n;
    const unsigned char *p;
    char *dst;
    if(!u32(c, &n))
        return 0;
    if(nullable && n == UINT32_MAX) {
        if(out)
            *out = NULL;
        return 1;
    }
    if(!take(c, n, &p) || !utf8(p, n))
        return 0;
    if(!out)
        return 1;
    dst = (char *)reserve(a, (size_t)n + 1, 1);
    if(!dst)
        return 0;
    if(a->base) {
        memcpy(dst, p, n);
        dst[n] = 0;
        *out = dst;
    }
    return 1;
}

static uint64_t dir_id(const WorldStore *s, uint32_t i)
{
    return le64(s->directory + (size_t)i * ENTRY_BYTES);
}

static uint32_t dir_kind(const WorldStore *s, uint32_t i)
{
    return le32(s->directory + (size_t)i * ENTRY_BYTES + 8);
}

static int locate(const WorldStore *s, uint64_t id, uint32_t kind)
{
    uint32_t lo = 0, hi = s->count;
    if(!id)
        return -1;
    while(lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(dir_id(s, mid) < id)
            lo = mid + 1;
        else
            hi = mid;
    }
    if(lo < s->count && dir_id(s, lo) == id && dir_kind(s, lo) == kind)
        return (int)lo;
    return -1;
}

static int bounded_count(Cursor *c, uint32_t *n, uint32_t minimum)
{
    return u32(c, n) && *n <= (c->size - c->at) / minimum;
}

static int parse(WorldStore *s, uint32_t item, ActiveSystem *out, Arena *a)
{
    Cursor c = {s->scratch,
                le32(s->directory + (size_t)item * ENTRY_BYTES + 16), 0};
    uint32_t kind = dir_kind(s, item);
    uint32_t n, i, j, start, flags;
    uint64_t id;
    const char *sprite;
    int32_t index, parent, price;

    if(!str(&c, 0, a, out ? &out->name : NULL))
        return 0;
    if(kind == 3) {
        if(!i32(&c, &index) || !i32(&c, &parent) || !i32(&c, &price))
            return 0;
        return c.at == c.size;
    }
    if(!str(&c, 0, a, out ? &out->display : NULL) ||
       !u32(&c, &flags) || (flags & ~(kind == 1 ? 15u : 3u)))
        return 0;
    if(out)
        out->flags = flags;

    if(kind == 1) {
        double *values[8];
        if(out) {
            values[0] = &out->x;
            values[1] = &out->y;
            values[2] = &out->jump_range;
            values[3] = &out->arrival_hyper;
            values[4] = &out->arrival_jump;
            values[5] = &out->departure_hyper;
            values[6] = &out->departure_jump;
            values[7] = &out->habitable;
        }
        for(i = 0; i < 8; ++i)
            if(!f64(&c, out ? values[i] : NULL))
                return 0;
    }

    if(!bounded_count(&c, &n, 4))
        return 0;
    for(i = 0; i < n; ++i)
        if(!str(&c, 0, NULL, NULL))
            return 0;

    if(!bounded_count(&c, &n, 8))
        return 0;
    start = c.at;
    if(kind == 1 && out) {
        out->link_count = n;
        out->links = (uint64_t *)reserve(a, (size_t)n * sizeof(uint64_t), 8);
        if(n && !out->links)
            return 0;
    }
    for(i = 0; i < n; ++i) {
        if(!u64(&c, &id) || locate(s, id, 1) < 0)
            return 0;
        for(j = 0; j < i; ++j)
            if(le64(c.data + start + (size_t)j * 8) == id)
                return 0;
        if(kind == 1 && out && a->base)
            out->links[i] = id;
    }
    if(kind == 2)
        return c.at == c.size;

    if(!bounded_count(&c, &n, 44))
        return 0;
    if(out) {
        out->object_count = n;
        out->objects = (WorldObject *)reserve(a,
            (size_t)n * sizeof(WorldObject), 8);
        if(n && !out->objects)
            return 0;
    }
    for(i = 0; i < n; ++i) {
        WorldObject *object = out && a->base ? &out->objects[i] : NULL;
        if(!i32(&c, &index) || !i32(&c, &parent) || !u64(&c, &id) ||
           index != (int32_t)i ||
           (parent != -1 && (parent < 0 || parent >= index)) ||
           (id && locate(s, id, 2) < 0))
            return 0;
        if(object) {
            object->index = index;
            object->parent = parent;
            object->planet = id;
        }
        if(!f64(&c, object ? &object->distance : NULL) ||
           !f64(&c, object ? &object->period : NULL) ||
           !f64(&c, object ? &object->offset : NULL) ||
           !str(&c, 1, a, out ? &sprite : NULL))
            return 0;
        if(object)
            object->sprite = sprite;
    }

    if(!bounded_count(&c, &n, 12))
        return 0;
    start = c.at;
    if(out) {
        out->trade_count = n;
        out->trade = (WorldTrade *)reserve(a,
            (size_t)n * sizeof(WorldTrade), 8);
        if(n && !out->trade)
            return 0;
    }
    for(i = 0; i < n; ++i) {
        if(!u64(&c, &id) || locate(s, id, 3) < 0 || !i32(&c, &price))
            return 0;
        for(j = 0; j < i; ++j)
            if(le64(c.data + start + (size_t)j * 12) == id)
                return 0;
        if(out && a->base) {
            out->trade[i].commodity = id;
            out->trade[i].price = price;
        }
    }
    return c.at == c.size;
}

static int read_item(WorldStore *s, uint32_t i)
{
    const unsigned char *entry = s->directory + (size_t)i * ENTRY_BYTES;
    uint32_t offset = le32(entry + 12);
    uint32_t length = le32(entry + 16);
    if(fseek(s->file, (long)offset, SEEK_SET) ||
       fread(s->scratch, 1, length, s->file) != length ||
       fnv(s->scratch, length) != le32(entry + 20)) {
        s->error = "record read/checksum failed";
        return 0;
    }
    return 1;
}

WorldStore *world_open(const char *path)
{
    unsigned char header[HEADER_BYTES];
    WorldStore *s;
    uint32_t count, max_record, payload_start, file_size, i, next;
    uint64_t previous_id = 0, one_bits;
    double one = 1.0;
    long actual_size;

    if(!path || sizeof(double) != 8)
        return NULL;
    memcpy(&one_bits, &one, 8);
    if(one_bits != UINT64_C(0x3ff0000000000000))
        return NULL;
    s = (WorldStore *)calloc(1, sizeof(*s));
    if(!s)
        return NULL;
    s->file = fopen(path, "rb");
    if(!s->file || fread(header, 1, HEADER_BYTES, s->file) != HEADER_BYTES ||
       memcmp(header, "ESWORLD1", 8))
        goto fail;

    count = le32(header + 8);
    max_record = le32(header + 12);
    payload_start = le32(header + 16);
    file_size = le32(header + 20);
    if(!count || count > MAX_RECORDS || !max_record ||
       max_record > MAX_RECORD_BYTES ||
       (size_t)count * ENTRY_BYTES + max_record + sizeof(*s) > STORE_BUDGET ||
       payload_start != HEADER_BYTES + count * ENTRY_BYTES ||
       file_size < payload_start || fseek(s->file, 0, SEEK_END) ||
       (actual_size = ftell(s->file)) < 0 ||
       (uint64_t)actual_size != file_size ||
       fseek(s->file, HEADER_BYTES, SEEK_SET))
        goto fail;

    s->count = count;
    s->max_record = max_record;
    s->bytes = sizeof(*s) + (size_t)count * ENTRY_BYTES + max_record;
    s->directory = (unsigned char *)malloc((size_t)count * ENTRY_BYTES);
    s->scratch = (unsigned char *)malloc(max_record);
    if(!s->directory || !s->scratch ||
       fread(s->directory, ENTRY_BYTES, count, s->file) != count)
        goto fail;

    next = payload_start;
    for(i = 0; i < count; ++i) {
        const unsigned char *entry = s->directory + (size_t)i * ENTRY_BYTES;
        uint64_t id = le64(entry);
        uint32_t kind = le32(entry + 8);
        uint32_t offset = le32(entry + 12);
        uint32_t length = le32(entry + 16);
        if(!id || id <= previous_id || kind < 1 || kind > 3 ||
           offset != next || length > max_record || length > file_size - next)
            goto fail;
        previous_id = id;
        next += length;
    }
    if(next != file_size)
        goto fail;
    for(i = 0; i < count; ++i)
        if(!read_item(s, i) || !parse(s, i, NULL, NULL))
            goto fail;
    s->error = "ok";
    return s;

fail:
    world_close(s);
    return NULL;
}

void world_close(WorldStore *s)
{
    if(!s)
        return;
    if(s->file)
        fclose(s->file);
    free(s->directory);
    free(s->scratch);
    free(s);
}

int world_load_system(WorldStore *s, uint64_t id, ActiveSystem *out)
{
    int index;
    Arena arena = {NULL, 0};
    ActiveSystem decoded = {0};
    void *memory;
    if(!s || !out || out->allocation || out->id || out->name ||
       out->display || out->objects || out->links || out->trade)
        return 0;
    index = locate(s, id, 1);
    if(index < 0) {
        s->error = "system ID not found";
        return 0;
    }
    if(!read_item(s, (uint32_t)index) ||
       !parse(s, (uint32_t)index, &decoded, &arena)) {
        s->error = "invalid system record";
        return 0;
    }
    memory = malloc(arena.used ? arena.used : 1);
    if(!memory) {
        s->error = "active allocation failed";
        return 0;
    }
    arena.base = (unsigned char *)memory;
    arena.used = 0;
    memset(&decoded, 0, sizeof(decoded));
    if(!parse(s, (uint32_t)index, &decoded, &arena)) {
        free(memory);
        s->error = "invalid system record";
        return 0;
    }
    decoded.id = id;
    decoded.allocation = memory;
    decoded.allocation_bytes = arena.used;
    *out = decoded;
    s->error = "ok";
    return 1;
}

void world_release_system(ActiveSystem *system)
{
    if(!system)
        return;
    free(system->allocation);
    memset(system, 0, sizeof(*system));
}

int world_find_system(WorldStore *s, const char *name, uint64_t *out)
{
    uint32_t i;
    size_t wanted;
    if(!s || !name || !out)
        return 0;
    wanted = strlen(name);
    for(i = 0; i < s->count; ++i) {
        uint32_t length, actual;
        if(dir_kind(s, i) != 1)
            continue;
        if(!read_item(s, i))
            return 0;
        length = le32(s->directory + (size_t)i * ENTRY_BYTES + 16);
        if(length < 4) {
            s->error = "invalid system name length";
            return 0;
        }
        actual = le32(s->scratch);
        if(actual > length - 4) {
            s->error = "invalid system name length";
            return 0;
        }
        if(actual == wanted && memcmp(s->scratch + 4, name, wanted) == 0) {
            *out = dir_id(s, i);
            s->error = "ok";
            return 1;
        }
    }
    s->error = "system name not found";
    return 0;
}

size_t world_store_bytes(const WorldStore *s)
{
    return s ? s->bytes : 0;
}

const char *world_error(const WorldStore *s)
{
    return s ? s->error : "world store unavailable";
}
