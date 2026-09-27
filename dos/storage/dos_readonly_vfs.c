/* SPDX-License-Identifier: GPL-3.0-or-later
 * Read-only SQLite VFS for an immutable, prebuilt DOS data artifact.
 * Compile the amalgamation with SQLITE_OS_OTHER=1 and SQLITE_THREADSAFE=0.
 * Concurrent writers, hot journals and mutable databases are unsupported.
 */
#include "sqlite3.h"
#include "dos_readonly_vfs.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct DosFile {
    sqlite3_file base;
    FILE *stream;
} DosFile;

static unsigned long long read_calls;
static unsigned long long read_bytes;

unsigned long long dos_vfs_reads(void) { return read_calls; }
unsigned long long dos_vfs_bytes(void) { return read_bytes; }
void dos_vfs_reset(void) { read_calls = read_bytes = 0; }

static int dos_close(sqlite3_file *file)
{
    DosFile *f = (DosFile *)file;
    int result = fclose(f->stream);
    f->stream = NULL;
    return result == 0 ? SQLITE_OK : SQLITE_IOERR_CLOSE;
}

static int dos_read(sqlite3_file *file, void *buffer, int amount,
                    sqlite3_int64 offset)
{
    DosFile *f = (DosFile *)file;
    size_t got;
    if (amount < 0 || offset < 0 || offset > LONG_MAX ||
        amount > LONG_MAX - offset)
        return SQLITE_IOERR_SEEK;
    if (fseek(f->stream, (long)offset, SEEK_SET) != 0)
        return SQLITE_IOERR_SEEK;
    got = fread(buffer, 1, (size_t)amount, f->stream);
    ++read_calls;
    read_bytes += (unsigned long long)got;
    if (got == (size_t)amount)
        return SQLITE_OK;
    memset((char *)buffer + got, 0, (size_t)amount - got);
    return ferror(f->stream) ? SQLITE_IOERR_READ : SQLITE_IOERR_SHORT_READ;
}

static int dos_write(sqlite3_file *file, const void *buffer, int amount,
                     sqlite3_int64 offset)
{
    (void)file; (void)buffer; (void)amount; (void)offset;
    return SQLITE_READONLY;
}

static int dos_truncate(sqlite3_file *file, sqlite3_int64 size)
{
    (void)file; (void)size;
    return SQLITE_READONLY;
}

static int dos_sync(sqlite3_file *file, int flags)
{
    (void)file; (void)flags;
    return SQLITE_OK;
}

static int dos_filesize(sqlite3_file *file, sqlite3_int64 *size)
{
    DosFile *f = (DosFile *)file;
    long end;
    if (fseek(f->stream, 0, SEEK_END) != 0)
        return SQLITE_IOERR_FSTAT;
    end = ftell(f->stream);
    if (end < 0)
        return SQLITE_IOERR_FSTAT;
    *size = (sqlite3_int64)end;
    return SQLITE_OK;
}

static int dos_lock(sqlite3_file *file, int level)
{
    (void)file; (void)level;
    return SQLITE_OK; /* Safe only while the artifact has no writer. */
}

static int dos_unlock(sqlite3_file *file, int level)
{
    (void)file; (void)level;
    return SQLITE_OK;
}

static int dos_reserved(sqlite3_file *file, int *reserved)
{
    (void)file;
    *reserved = 0;
    return SQLITE_OK;
}

static int dos_control(sqlite3_file *file, int op, void *arg)
{
    (void)file; (void)op; (void)arg;
    return SQLITE_NOTFOUND;
}

static int dos_sector(sqlite3_file *file) { (void)file; return 512; }
static int dos_characteristics(sqlite3_file *file)
{
    (void)file;
    return SQLITE_IOCAP_IMMUTABLE;
}

static const sqlite3_io_methods dos_io = {
    .iVersion = 1,
    .xClose = dos_close,
    .xRead = dos_read,
    .xWrite = dos_write,
    .xTruncate = dos_truncate,
    .xSync = dos_sync,
    .xFileSize = dos_filesize,
    .xLock = dos_lock,
    .xUnlock = dos_unlock,
    .xCheckReservedLock = dos_reserved,
    .xFileControl = dos_control,
    .xSectorSize = dos_sector,
    .xDeviceCharacteristics = dos_characteristics
};

static int dos_open(sqlite3_vfs *vfs, const char *name, sqlite3_file *file,
                    int flags, int *out_flags)
{
    DosFile *f = (DosFile *)file;
    (void)vfs;
    memset(f, 0, sizeof(*f));
    if (out_flags)
        *out_flags = 0;
    if (!(flags & SQLITE_OPEN_MAIN_DB) || !name)
        return SQLITE_CANTOPEN;
    if (!(flags & SQLITE_OPEN_READONLY) ||
        (flags & (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)))
        return SQLITE_READONLY;
    f->stream = fopen(name, "rb");
    if (!f->stream)
        return SQLITE_CANTOPEN;
    f->base.pMethods = &dos_io;
    if (out_flags)
        *out_flags = SQLITE_OPEN_READONLY;
    return SQLITE_OK;
}

static int dos_delete(sqlite3_vfs *vfs, const char *name, int sync_dir)
{
    (void)vfs; (void)name; (void)sync_dir;
    return SQLITE_IOERR_DELETE;
}

static int dos_access(sqlite3_vfs *vfs, const char *name, int flags, int *result)
{
    FILE *f;
    struct stat info;
    (void)vfs;
    *result = 0;
    if (!name)
        return SQLITE_OK;
    if (flags == SQLITE_ACCESS_EXISTS) {
        *result = stat(name, &info) == 0;
        return SQLITE_OK;
    }
    if (flags == SQLITE_ACCESS_READWRITE)
        f = fopen(name, "rb+");
    else if (flags == SQLITE_ACCESS_READ)
        f = fopen(name, "rb");
    else
        return SQLITE_IOERR_ACCESS;
    if (f) {
        *result = 1;
        fclose(f);
    }
    return SQLITE_OK;
}

static int dos_fullpath(sqlite3_vfs *vfs, const char *name, int length, char *out)
{
    size_t prefix = 0, suffix;
    (void)vfs;
    if (!name || length <= 0)
        return SQLITE_CANTOPEN;
    /* Resolving a drive-relative name requires DOS per-drive cwd state. */
    if (name[0] && name[1] == ':' &&
        name[2] != '/' && name[2] != '\\')
        return SQLITE_CANTOPEN;
    /* DOS absolute paths may start with a drive (C:\\...) or root slash. */
    if (name[0] != '/' && name[0] != '\\' &&
        !(name[0] && name[1] == ':' &&
          (name[2] == '/' || name[2] == '\\'))) {
        if (!getcwd(out, (size_t)length))
            return SQLITE_CANTOPEN;
        prefix = strlen(out);
        if (prefix + 1 >= (size_t)length)
            return SQLITE_CANTOPEN;
        out[prefix++] = '/';
    }
    suffix = strlen(name);
    if (suffix >= (size_t)length - prefix)
        return SQLITE_CANTOPEN;
    memcpy(out + prefix, name, suffix + 1);
    return SQLITE_OK;
}

static void *dos_dl_open(sqlite3_vfs *vfs, const char *name)
{ (void)vfs; (void)name; return NULL; }
static void dos_dl_error(sqlite3_vfs *vfs, int length, char *out)
{
    const char *message = "extension loading unavailable";
    (void)vfs;
    if (length > 0) {
        size_t n = strlen(message);
        if (n >= (size_t)length) n = (size_t)length - 1;
        memcpy(out, message, n);
        out[n] = '\0';
    }
}
static void (*dos_dl_sym(sqlite3_vfs *vfs, void *handle,
                         const char *symbol))(void)
{ (void)vfs; (void)handle; (void)symbol; return NULL; }
static void dos_dl_close(sqlite3_vfs *vfs, void *handle)
{ (void)vfs; (void)handle; }

static int dos_randomness(sqlite3_vfs *vfs, int length, char *out)
{
    static unsigned long state;
    int i;
    (void)vfs;
    if (!state) state = (unsigned long)time(NULL) ^ (unsigned long)clock() ^ 0x9e3779b9UL;
    for (i = 0; i < length; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        out[i] = (char)(state & 255);
    }
    return length;
}

static int dos_sleep(sqlite3_vfs *vfs, int microseconds)
{
    clock_t start, ticks;
    (void)vfs;
    if (microseconds <= 0) return 0;
    start = clock();
    ticks = (clock_t)((double)microseconds * CLOCKS_PER_SEC / 1000000.0);
    while (clock() - start < ticks) { }
    return microseconds;
}

static int dos_current_time(sqlite3_vfs *vfs, double *julian)
{
    time_t now = time(NULL);
    (void)vfs;
    if (now == (time_t)-1) return SQLITE_ERROR;
    *julian = 2440587.5 + (double)now / 86400.0;
    return SQLITE_OK;
}

static int dos_last_error(sqlite3_vfs *vfs, int length, char *out)
{
    (void)vfs;
    if (length > 0 && out) {
        const char *message = strerror(errno);
        size_t n = strlen(message);
        if (n >= (size_t)length) n = (size_t)length - 1;
        memcpy(out, message, n);
        out[n] = '\0';
    }
    return errno;
}

static sqlite3_vfs dos_vfs = {
    .iVersion = 1,
    .szOsFile = sizeof(DosFile),
    .mxPathname = 1024,
    .zName = "dos-ro",
    .xOpen = dos_open,
    .xDelete = dos_delete,
    .xAccess = dos_access,
    .xFullPathname = dos_fullpath,
    .xDlOpen = dos_dl_open,
    .xDlError = dos_dl_error,
    .xDlSym = dos_dl_sym,
    .xDlClose = dos_dl_close,
    .xRandomness = dos_randomness,
    .xSleep = dos_sleep,
    .xCurrentTime = dos_current_time,
    .xGetLastError = dos_last_error
};

int sqlite3_os_init(void) { return sqlite3_vfs_register(&dos_vfs, 1); }
int sqlite3_os_end(void) { return sqlite3_vfs_unregister(&dos_vfs); }
