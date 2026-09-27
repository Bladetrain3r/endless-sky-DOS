/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __DJGPP__
#include <dpmi.h>
#endif

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static double milliseconds(void)
{
#ifdef __DJGPP__
    return (double)uclock()*1000.0/UCLOCKS_PER_SEC;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec*1000.0+t.tv_nsec/1000000.0;
#endif
}
static unsigned long free_pages(void)
{
#ifdef __DJGPP__
    __dpmi_free_mem_info m;
    if(!__dpmi_get_free_memory_information(&m)) return m.total_number_of_free_pages;
#endif
    return 0;
}
static uint32_t hash(const unsigned char *data, uint32_t n)
{
    uint32_t h=2166136261u, i;
    for(i=0;i<n;++i) h=(h^data[i])*16777619u;
    return h;
}
static int phase(const char *name, const char *path)
{
    unsigned char header[12], entry[12];
    uint32_t i, count;
    uint64_t payload_bytes=0, before_reads=store_reads(), before_bytes=store_bytes();
    double start;
    FILE *trace=fopen(path,"rb");
    if(!trace || fread(header,1,12,trace)!=12 || memcmp(header,"ESTRACE1",8))
    { puts("error=bad_trace"); if(trace) fclose(trace); return 0; }
    count=le32(header+8);
    if(!count || count>100000) { fclose(trace); return 0; }
    start=milliseconds();
    for(i=0;i<count;++i)
    {
        uint32_t id, expected_size, expected_hash, length;
        const unsigned char *data;
        if(fread(entry,1,12,trace)!=12) { fclose(trace); return 0; }
        id=le32(entry); expected_size=le32(entry+4); expected_hash=le32(entry+8);
        data=store_get(id,&length);
        if(!data || length!=expected_size || hash(data,length)!=expected_hash)
        {
            printf("error=payload_mismatch:%lu:%s\n",(unsigned long)id,store_error());
            fclose(trace); return 0;
        }
        payload_bytes += length;
    }
    printf("%s_ms=%.3f\n%s_queries=%lu\n%s_payload_bytes=%llu\n",name,milliseconds()-start,
           name,(unsigned long)count,name,(unsigned long long)payload_bytes);
    printf("%s_store_reads=%llu\n%s_store_bytes=%llu\n",name,
           (unsigned long long)(store_reads()-before_reads),name,
           (unsigned long long)(store_bytes()-before_bytes));
    if(fgetc(trace)!=EOF) { fclose(trace); return 0; }
    fclose(trace);
    return 1;
}
int main(int argc, char **argv)
{
    double start;
    uint32_t length;
    int ok;
    if(argc!=3) { puts("usage=STORE.EXE filename cache_kib"); return 1; }
    printf("probe=content_storage\nfree_pages_before=%lu\n",free_pages());
    start=milliseconds();
    if(!store_open(argv[1],atoi(argv[2])))
    { printf("error=open:%s\n",store_error()); store_close(); return 2; }
    printf("open_ms=%.3f\nopen_store_reads=%llu\nopen_store_bytes=%llu\n",
           milliseconds()-start,(unsigned long long)store_reads(),(unsigned long long)store_bytes());
    ok=phase("seq","SEQ.BIN") && phase("random","RAND.BIN") && phase("hot","HOT.BIN");
    if(store_get(UINT32_MAX,&length)!=NULL || length!=0) ok=0;
    if(!store_check_readonly()) { puts("error=write_not_refused"); ok=0; }
    printf("memory_peak_bytes=%llu\nmemory_scope=%s\nfree_pages_after=%lu\n",
           (unsigned long long)store_memory_peak(),store_memory_label(),free_pages());
    store_close();
    puts(ok ? "status=ok" : "status=failed");
    return ok ? 0 : 3;
}
