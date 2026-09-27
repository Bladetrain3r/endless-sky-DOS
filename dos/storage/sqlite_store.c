/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "store.h"
#include "sqlite3.h"
#include "dos_readonly_vfs.h"
#include <stdio.h>
#include <string.h>

static sqlite3 *db;
static sqlite3_stmt *lookup;
static char error[160];
static uint64_t peak;

static int check(int rc)
{
    if(rc == SQLITE_OK) return 1;
    snprintf(error,sizeof(error),"%d:%s",rc,db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
    return 0;
}

int store_open(const char *path, int cache_kib)
{
    char settings[128];
    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
    sqlite3_config(SQLITE_CONFIG_MEMSTATUS,1);
    error[0]=0;
    if(!check(sqlite3_initialize())) return 0;
    sqlite3_hard_heap_limit64(1024*1024);
    sqlite3_memory_highwater(1);
    dos_vfs_reset();
    if(!check(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,"dos-ro"))) return 0;
    snprintf(settings,sizeof(settings),
        "PRAGMA cache_size=-%d; PRAGMA mmap_size=0; PRAGMA query_only=ON;",cache_kib);
    if(!check(sqlite3_exec(db,settings,NULL,NULL,NULL))) return 0;
    return check(sqlite3_prepare_v2(db,"SELECT payload FROM records WHERE id=?1",-1,&lookup,NULL));
}
const unsigned char *store_get(uint32_t id, uint32_t *length)
{
    int rc;
    *length=0;
    if(!check(sqlite3_reset(lookup)) || !check(sqlite3_bind_int64(lookup,1,id))) return NULL;
    rc=sqlite3_step(lookup);
    if(rc==SQLITE_DONE) return NULL;
    if(rc!=SQLITE_ROW) { check(rc); return NULL; }
    *length=(uint32_t)sqlite3_column_bytes(lookup,0);
    return sqlite3_column_blob(lookup,0);
}
int store_check_readonly(void)
{
    int rc;
    sqlite3_reset(lookup);
    rc=sqlite3_exec(db,"UPDATE records SET payload=zeroblob(1) WHERE id=0",NULL,NULL,NULL);
    return rc==SQLITE_READONLY;
}
void store_close(void)
{
    peak=(uint64_t)sqlite3_memory_highwater(0);
    sqlite3_finalize(lookup); lookup=NULL;
    sqlite3_close(db); db=NULL;
    sqlite3_shutdown();
}
uint64_t store_reads(void) { return dos_vfs_reads(); }
uint64_t store_bytes(void) { return dos_vfs_bytes(); }
uint64_t store_memory_peak(void) { return db ? (uint64_t)sqlite3_memory_highwater(0) : peak; }
const char *store_memory_label(void) { return "sqlite_allocator_peak_excludes_stdio_runtime"; }
const char *store_error(void) { return error; }
