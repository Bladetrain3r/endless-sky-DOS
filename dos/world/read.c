/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded, read-only validator for the ESWORLD1 typed snapshot. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __DJGPP__
#include <dpmi.h>
#endif

enum { HEADER_SIZE=24, ENTRY_SIZE=24, MAX_RECORDS=8192,
       MAX_RECORD_BYTES=32768, MAX_HEAP_BYTES=262144 };

typedef struct {
    const unsigned char *p;
    uint32_t length, pos;
} Cursor;
typedef struct {
    uint32_t records, systems, planets, commodities, links, objects, trades,
             memberships, payload_hash;
} Stats;

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 |
           (uint32_t)p[3]<<24;
}
static uint64_t le64(const unsigned char *p)
{
    return (uint64_t)le32(p) | (uint64_t)le32(p+4)<<32;
}
static uint32_t fnv(uint32_t h, const unsigned char *p, uint32_t n)
{
    uint32_t i;
    for(i=0;i<n;++i) h=(h^p[i])*16777619u;
    return h;
}
static double milliseconds(void)
{
#ifdef __DJGPP__
    return (double)uclock()*1000.0/UCLOCKS_PER_SEC;
#else
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)) return 0.0;
    return (double)t.tv_sec*1000.0+(double)t.tv_nsec/1000000.0;
#endif
}
static unsigned long free_pages(void)
{
#ifdef __DJGPP__
    __dpmi_free_mem_info m;
    if(!__dpmi_get_free_memory_information(&m))
        return m.total_number_of_free_pages;
#endif
    return 0;
}
static int take(Cursor *c, uint32_t n, const unsigned char **p)
{
    if(n>c->length-c->pos) return 0;
    *p=c->p+c->pos;
    c->pos+=n;
    return 1;
}
static int u32(Cursor *c, uint32_t *v)
{
    const unsigned char *p;
    if(!take(c,4,&p)) return 0;
    *v=le32(p);
    return 1;
}
static int i32(Cursor *c, int32_t *v)
{
    uint32_t bits;
    if(!u32(c,&bits)) return 0;
    /* Conversion from an out-of-range unsigned value is implementation-defined. */
    *v=bits<=INT32_MAX ? (int32_t)bits :
       (int32_t)((int64_t)bits-INT64_C(4294967296));
    return 1;
}
static int u64(Cursor *c, uint64_t *v)
{
    const unsigned char *p;
    if(!take(c,8,&p)) return 0;
    *v=le64(p);
    return 1;
}
static int f64(Cursor *c)
{
    uint64_t bits;
    double value;
    /* Validate the wire value; this reader does not retain decoded objects. */
    if(!u64(c,&bits) || (bits&UINT64_C(0x7ff0000000000000))==
                        UINT64_C(0x7ff0000000000000)) return 0;
    memcpy(&value,&bits,8);
    (void)value;
    return 1;
}
/* Reject invalid UTF-8, embedded NUL, surrogates and non-scalar values. */
static int utf8(const unsigned char *p, uint32_t n)
{
    uint32_t i=0;
    while(i<n) {
        uint32_t cp, low, need, j;
        unsigned char b=p[i++];
        if(!b) return 0;
        if(b<0x80) continue;
        if(b>=0xc2 && b<=0xdf) { cp=b&31; low=0x80; need=1; }
        else if(b>=0xe0 && b<=0xef) { cp=b&15; low=0x800; need=2; }
        else if(b>=0xf0 && b<=0xf4) { cp=b&7; low=0x10000; need=3; }
        else return 0;
        if(need>n-i) return 0;
        for(j=0;j<need;++j) {
            b=p[i++];
            if((b&0xc0)!=0x80) return 0;
            cp=(cp<<6)|(b&63);
        }
        if(cp<low || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
    }
    return 1;
}
static int string(Cursor *c, int nullable)
{
    uint32_t n;
    const unsigned char *p;
    if(!u32(c,&n)) return 0;
    if(nullable && n==UINT32_MAX) return 1;
    return take(c,n,&p) && utf8(p,n);
}
static uint64_t entry_id(const unsigned char *dir, uint32_t i)
{
    return le64(dir+(size_t)i*ENTRY_SIZE);
}
static uint32_t entry_kind(const unsigned char *dir, uint32_t i)
{
    return le32(dir+(size_t)i*ENTRY_SIZE+8);
}
static int reference(const unsigned char *dir, uint32_t count,
                     uint64_t id, uint32_t kind)
{
    uint32_t lo=0, hi=count;
    if(!id) return 0;
    while(lo<hi) {
        uint32_t mid=lo+(hi-lo)/2;
        uint64_t found=entry_id(dir,mid);
        if(found<id) lo=mid+1;
        else hi=mid;
    }
    return lo<count && entry_id(dir,lo)==id && entry_kind(dir,lo)==kind;
}
static int bounded_count(Cursor *c, uint32_t *count, uint32_t min_size)
{
    return u32(c,count) && *count <= (c->length-c->pos)/min_size;
}
static int parse_system(Cursor *c, const unsigned char *dir, uint32_t total, Stats *s)
{
    uint32_t flags, count, i, j, list_start;
    uint64_t id;
    int32_t index, parent, price;
    if(!string(c,0) || !u32(c,&flags) || (flags&~15u)) return 0;
    for(i=0;i<8;++i) if(!f64(c)) return 0;
    if(!bounded_count(c,&count,4)) return 0;
    for(i=0;i<count;++i) if(!string(c,0)) return 0;
    if(!bounded_count(c,&count,8)) return 0;
    list_start=c->pos;
    for(i=0;i<count;++i) {
        if(!u64(c,&id) || !reference(dir,total,id,1)) return 0;
        for(j=0;j<i;++j)
            if(le64(c->p+list_start+(size_t)j*8)==id) return 0;
    }
    s->links+=count;
    if(!bounded_count(c,&count,44)) return 0;
    for(i=0;i<count;++i) {
        if(!i32(c,&index) || !i32(c,&parent) || !u64(c,&id) ||
           index!=(int32_t)i || (parent!=-1 && (parent<0 || parent>=index)) ||
           (id && !reference(dir,total,id,2))) return 0;
        for(j=0;j<3;++j) if(!f64(c)) return 0;
        if(!string(c,1)) return 0;
    }
    s->objects+=count;
    if(!bounded_count(c,&count,12)) return 0;
    list_start=c->pos;
    for(i=0;i<count;++i) {
        if(!u64(c,&id) || !reference(dir,total,id,3) || !i32(c,&price)) return 0;
        for(j=0;j<i;++j)
            if(le64(c->p+list_start+(size_t)j*12)==id) return 0;
    }
    s->trades+=count;
    ++s->systems;
    return 1;
}
static int parse_planet(Cursor *c, const unsigned char *dir, uint32_t total, Stats *s)
{
    uint32_t flags, count, i, j, list_start;
    uint64_t id;
    if(!string(c,0) || !u32(c,&flags) || (flags&~3u)) return 0;
    if(!bounded_count(c,&count,4)) return 0;
    for(i=0;i<count;++i) if(!string(c,0)) return 0;
    if(!bounded_count(c,&count,8)) return 0;
    list_start=c->pos;
    for(i=0;i<count;++i) {
        if(!u64(c,&id) || !reference(dir,total,id,1)) return 0;
        for(j=0;j<i;++j)
            if(le64(c->p+list_start+(size_t)j*8)==id) return 0;
    }
    s->memberships+=count;
    ++s->planets;
    return 1;
}
static int parse_record(const unsigned char *payload, uint32_t length,
                        const unsigned char *dir, uint32_t total,
                        uint32_t kind, Stats *s)
{
    Cursor c;
    int32_t ordinal, low, high;
    c.p=payload; c.length=length; c.pos=0;
    if(!string(&c,0)) return 0;
    if(kind==1) {
        if(!parse_system(&c,dir,total,s)) return 0;
    } else if(kind==2) {
        if(!parse_planet(&c,dir,total,s)) return 0;
    } else if(kind==3) {
        if(!i32(&c,&ordinal) || !i32(&c,&low) || !i32(&c,&high)) return 0;
        ++s->commodities;
    } else return 0;
    return c.pos==c.length;
}
static int validate(FILE *f, Stats *s, size_t *peak)
{
    unsigned char header[HEADER_SIZE], *dir=NULL, *payload=NULL;
    uint32_t count, maxrec, dataoff, filesize, i, next;
    uint64_t prev_id=0;
    uint64_t one_bits;
    double one=1.0;
    long actual;
    int ok=0;
    if(sizeof(double)!=8 || sizeof(uint64_t)!=8) return 0;
    memcpy(&one_bits,&one,8);
    if(one_bits!=UINT64_C(0x3ff0000000000000) ||
       fread(header,1,HEADER_SIZE,f)!=HEADER_SIZE ||
       memcmp(header,"ESWORLD1",8)) return 0;
    count=le32(header+8); maxrec=le32(header+12);
    dataoff=le32(header+16); filesize=le32(header+20);
    if(!count || !maxrec || count>MAX_RECORDS || maxrec>MAX_RECORD_BYTES ||
       (size_t)count*ENTRY_SIZE+(size_t)maxrec>MAX_HEAP_BYTES ||
       dataoff!=HEADER_SIZE+count*ENTRY_SIZE || filesize<dataoff ||
       fseek(f,0,SEEK_END) || (actual=ftell(f))<0 ||
       (uint64_t)actual!=filesize || fseek(f,HEADER_SIZE,SEEK_SET)) return 0;
    *peak=(count ? (size_t)count*ENTRY_SIZE : 1)+
          (maxrec ? (size_t)maxrec : 1);
    dir=(unsigned char *)malloc(count ? (size_t)count*ENTRY_SIZE : 1);
    payload=(unsigned char *)malloc(maxrec ? maxrec : 1);
    if(!dir || !payload ||
       fread(dir,ENTRY_SIZE,count,f)!=count) goto done;
    next=dataoff;
    for(i=0;i<count;++i) {
        const unsigned char *e=dir+(size_t)i*ENTRY_SIZE;
        uint64_t id=le64(e);
        uint32_t kind=le32(e+8), off=le32(e+12), len=le32(e+16);
        if(!id || id<=prev_id || kind<1 || kind>3 ||
           off!=next || len>maxrec || len>filesize-next) goto done;
        prev_id=id; next+=len;
    }
    if(next!=filesize) goto done;
    s->records=count;
    s->payload_hash=2166136261u;
    for(i=0;i<count;++i) {
        const unsigned char *e=dir+(size_t)i*ENTRY_SIZE;
        uint32_t length=le32(e+16);
        if(fread(payload,1,length,f)!=length ||
           fnv(2166136261u,payload,length)!=le32(e+20) ||
           !parse_record(payload,length,dir,count,le32(e+8),s)) goto done;
        s->payload_hash=fnv(s->payload_hash,payload,length);
    }
    ok=1;
done:
    free(payload); free(dir);
    return ok;
}
int main(int argc, char **argv)
{
    FILE *f=NULL;
    Stats s={0};
    size_t peak=0;
    unsigned long before=free_pages(), after;
    double start=milliseconds(), elapsed;
    int ok=0;
    if(argc==2) {
        f=fopen(argv[1],"rb");
        if(f) { ok=validate(f,&s,&peak); fclose(f); }
    }
    elapsed=milliseconds()-start;
    after=free_pages();
    printf("status=%s\nrecords=%lu\nsystems=%lu\nplanets=%lu\ncommodities=%lu\n"
           "links=%lu\nobjects=%lu\ntrades=%lu\nmemberships=%lu\n"
           "payload_hash_hex=%08lx\nmemory_peak_bytes=%lu\n"
           "free_pages_before=%lu\nfree_pages_after=%lu\nvalidation_ms=%.3f\n",
           ok?"ok":"failed",(unsigned long)s.records,(unsigned long)s.systems,
           (unsigned long)s.planets,(unsigned long)s.commodities,
           (unsigned long)s.links,(unsigned long)s.objects,(unsigned long)s.trades,
           (unsigned long)s.memberships,(unsigned long)s.payload_hash,
           (unsigned long)peak,before,after,elapsed);
    return ok?0:1;
}
