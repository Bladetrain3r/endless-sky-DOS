/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static FILE *file;
static unsigned char *index_data, *payload;
static uint32_t count, maximum;
static uint64_t reads, bytes, allocations;
/* A bounded FIFO ring: direct record->slot hits; at most64 overlap checks/miss.
 * Whole records only. Long records fall back to the reusable payload buffer. */
struct CacheSlot { uint32_t id, offset, length; };
static struct CacheSlot *slots;
static uint32_t *cached_slot;
static unsigned char *cache;
static uint32_t cache_size, cache_cursor, next_slot;
static const char *error = "none";

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}

static int read_exact(void *buffer, size_t size)
{
    size_t got;
    ++reads;
    got = fread(buffer,1,size,file);
    bytes += got;
    if(got != size) { error = "short_read"; return 0; }
    return 1;
}

int store_open(const char *path, int cache_kib)
{
    unsigned char header[16];
    uint64_t expected;
    long size;
    uint32_t i;
    if(cache_kib < 0 || cache_kib > 256) { error="bad_cache_limit"; return 0; }
    reads = bytes = allocations = 0;
    error = "none";
    file = fopen(path,"rb");
    if(!file) { error = "open_failed"; return 0; }
    if(!read_exact(header,sizeof(header))) return 0;
    if(memcmp(header,"ESPACK1\0",8)) { error = "bad_header"; return 0; }
    count = le32(header+8);
    maximum = le32(header+12);
    if(!count || count > 100000 || !maximum || maximum > 262144)
    { error = "invalid_limits"; return 0; }
    if(fseek(file,0,SEEK_END) || (size=ftell(file)) < 0 || fseek(file,16,SEEK_SET))
    { error = "size_failed"; return 0; }
    expected = 16 + (uint64_t)count*8;
    if(expected > (uint64_t)size) { error = "truncated_index"; return 0; }
    index_data = malloc((size_t)count*8);
    payload = malloc(maximum);
    allocations = (uint64_t)count*8 + maximum;
    if(!index_data || !payload) { error = "allocation_failed"; return 0; }
    if(!read_exact(index_data,(size_t)count*8)) return 0;
    for(i=0;i<count;++i)
    {
        uint32_t offset=le32(index_data+8*i), length=le32(index_data+8*i+4);
        if(offset != expected || !length || length > maximum)
        { error = "invalid_record"; return 0; }
        expected += length;
        if(expected > (uint64_t)size || expected > LONG_MAX)
        { error = "record_out_of_bounds"; return 0; }
    }
    if(expected != (uint64_t)size) { error = "unexpected_trailing_bytes"; return 0; }
    if(cache_kib)
    {
        cache_size=(uint32_t)cache_kib*1024;
        cache=malloc(cache_size);
        slots=calloc(64,sizeof(*slots));
        cached_slot=calloc(count,sizeof(*cached_slot));
        allocations += cache_size + 64*sizeof(*slots) + (uint64_t)count*sizeof(*cached_slot);
        if(!cache || !slots || !cached_slot) { error="cache_allocation_failed"; return 0; }
        cache_cursor=next_slot=0;
    }
    return 1;
}

const unsigned char *store_get(uint32_t id, uint32_t *length)
{
    uint32_t offset, i;
    *length=0;
    if(id >= count) return NULL;
    offset=le32(index_data+8*id);
    *length=le32(index_data+8*id+4);
    if(cache && cached_slot[id])
        return cache + slots[cached_slot[id]-1].offset;
    if(cache && *length <= cache_size)
    {
        struct CacheSlot *slot;
        if(cache_cursor + *length > cache_size) cache_cursor=0;
        for(i=0;i<64;++i)
            if(slots[i].length && cache_cursor < slots[i].offset+slots[i].length
               && slots[i].offset < cache_cursor+*length)
            { cached_slot[slots[i].id]=0; slots[i].length=0; }
        slot=&slots[next_slot];
        if(slot->length) cached_slot[slot->id]=0;
        slot->length=0;
        if(fseek(file,(long)offset,SEEK_SET)) { error="seek_failed"; return NULL; }
        if(!read_exact(cache+cache_cursor,*length)) return NULL;
        slot->id=id; slot->offset=cache_cursor; slot->length=*length;
        cached_slot[id]=next_slot+1;
        next_slot=(next_slot+1)%64;
        cache_cursor += *length;
        return cache+slot->offset;
    }
    if(fseek(file,(long)offset,SEEK_SET)) { error="seek_failed"; return NULL; }
    if(!read_exact(payload,*length)) return NULL;
    return payload;
}
void store_close(void)
{
    free(index_data); free(payload); index_data=payload=NULL;
    if(file) fclose(file);
    file=NULL;
    free(slots); free(cache); free(cached_slot);
    slots=NULL; cache=NULL; cached_slot=NULL; cache_size=cache_cursor=next_slot=0;
}
uint64_t store_reads(void) { return reads; }
uint64_t store_bytes(void) { return bytes; }
uint64_t store_memory_peak(void) { return allocations; }
const char *store_memory_label(void) { return "explicit_index_payload_cache_excludes_stdio_runtime"; }
const char *store_error(void) { return error; }
int store_check_readonly(void) { return 1; } /* No write API; file opened rb. */
