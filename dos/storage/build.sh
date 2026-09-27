#!/bin/sh
# Invoked inside the configured tools container, working directory = repo root.
set -eu
cc=.work/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc
common='-std=gnu99 -Os -march=i386 -mtune=i586 -mno-mmx -mno-sse -mno-sse2 -ffunction-sections -fdata-sections'
defines='-DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_TEMP_STORE=3 -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_DQS=0'
includes='-I.work/storage/sqlite -Idos/storage'
mkdir -p .work/storage/run
$cc $common $defines -c .work/storage/sqlite/sqlite3.c -o .work/storage/sqlite/sqlite3.o
$cc $common -Wall -Wextra -Werror $includes dos/storage/bench.c dos/storage/pack_store.c \
    -Wl,--gc-sections -o .work/storage/run/PACK.EXE
$cc $common -Wall -Wextra -Werror $defines $includes dos/storage/bench.c dos/storage/sqlite_store.c \
    dos/storage/dos_readonly_vfs.c .work/storage/sqlite/sqlite3.o \
    -Wl,--gc-sections -o .work/storage/run/SQL.EXE
