#!/usr/bin/env python3
"""Native sanitizer contract tests for the two immutable storage readers."""
import hashlib
import os
from pathlib import Path
import sqlite3
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "dos/storage"
WORK = ROOT / ".work/storage/tests"
SQLITE = ROOT / ".work/storage/sqlite"
FLAGS = ["-std=gnu99", "-O1", "-g", "-fno-omit-frame-pointer",
         "-fsanitize=address,undefined", "-Wall", "-Wextra", "-Werror"]
SQL_FLAGS = ["-DSQLITE_OS_OTHER=1", "-DSQLITE_THREADSAFE=0",
             "-DSQLITE_OMIT_LOAD_EXTENSION=1", "-DSQLITE_OMIT_WAL=1",
             "-DSQLITE_TEMP_STORE=3", "-DSQLITE_MAX_MMAP_SIZE=0"]
INCLUDES = ["-I" + str(SOURCE), "-I" + str(SQLITE)]


def run(args, cwd=WORK, expected=0):
    env = os.environ.copy()
    # LeakSanitizer reported a ptrace fatal error here; leaks are not checked.
    env["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    result = subprocess.run([str(x) for x in args], cwd=cwd, env=env,
                            text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=120)
    if result.returncode != expected:
        raise AssertionError(f"{args[0]} exited {result.returncode}, "
                             f"expected {expected}:\n{result.stdout}")
    if "ERROR: AddressSanitizer" in result.stdout or "runtime error:" in result.stdout:
        raise AssertionError(result.stdout)
    return result.stdout


def fnv(data):
    h = 2166136261
    for byte in data:
        h = ((h ^ byte) * 16777619) & 0xffffffff
    return h


def fixture():
    # Deliberately include binary NULs and an EOF-adjacent last record.
    rows = [b"alpha\n", b"\0\xffbinary", bytes(range(256)), b"last"]
    (WORK / "CONTENT.DB").unlink(missing_ok=True)
    with sqlite3.connect(WORK / "CONTENT.DB") as db:
        db.execute("PRAGMA page_size=4096")
        db.execute("CREATE TABLE records(id INTEGER PRIMARY KEY, payload BLOB NOT NULL)")
        db.executemany("INSERT INTO records VALUES (?, ?)", enumerate(rows))
        assert [x[0] for x in db.execute("SELECT payload FROM records ORDER BY id")] == rows
    header = b"ESPACK1\0" + struct.pack("<II", len(rows), max(map(len, rows)))
    at = len(header) + len(rows) * 8
    index = bytearray()
    for row in rows:
        index += struct.pack("<II", at, len(row))
        at += len(row)
    pack = header + index + b"".join(rows)
    (WORK / "CONTENT.PAK").write_bytes(pack)
    for i, row in enumerate(rows):
        offset, length = struct.unpack_from("<II", pack, 16 + i * 8)
        assert pack[offset:offset + length] == row
    traces = {"SEQ": [0, 1, 2, 3], "RAND": [3, 0, 2, 1, 3, 2],
              "HOT": [1, 1, 1, 1, 1]}
    for name, ids in traces.items():
        trace = b"ESTRACE1" + struct.pack("<I", len(ids))
        trace += b"".join(struct.pack("<III", i, len(rows[i]), fnv(rows[i]))
                          for i in ids)
        (WORK / (name + ".BIN")).write_bytes(trace)
    (WORK / "TRUNC.PAK").write_bytes(pack[:-1])
    (WORK / "BAD.PAK").write_bytes(b"BROKEN!!" + pack[8:])
    invalid_index = bytearray(pack)
    struct.pack_into("<I", invalid_index, 16, len(pack) + 100)
    (WORK / "BAD_INDEX.PAK").write_bytes(invalid_index)
    bad_payload = bytearray(pack)
    bad_payload[len(header) + len(index)] ^= 1
    (WORK / "BAD_PAYLOAD.PAK").write_bytes(bad_payload)
    db_bytes = (WORK / "CONTENT.DB").read_bytes()
    (WORK / "TRUNC.DB").write_bytes(db_bytes[:100])
    (WORK / "BAD.DB").write_bytes(b"BROKEN!!" + db_bytes[8:])
    stress = [bytes((i + j) & 255 for j in range(1000)) for i in range(90)]
    stress.append(bytes(j & 255 for j in range(70000)))
    header = b"ESPACK1\0" + struct.pack("<II", len(stress), 70000)
    at = len(header) + len(stress) * 8
    index = bytearray()
    for row in stress:
        index += struct.pack("<II", at, len(row))
        at += len(row)
    (WORK / "STRESS.PAK").write_bytes(header + index + b"".join(stress))
    return rows


HARNESS = r'''
#include "store.h"
#include "sqlite3.h"
#include "dos_readonly_vfs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    uint32_t n = 99;
    const unsigned char *p;
    uint32_t i, j;
    uint64_t before;
    if (argc != 4) return 90;
    if (!strcmp(argv[1], "missing")) {
        if (store_open(argv[2], 64)) return 1;
        store_close();
        return 0;
    }
    if (!strcmp(argv[1], "badfile")) {
        if (store_open(argv[2], 64)) return 2;
        store_close();
        return 0;
    }
    if (!strcmp(argv[1], "stress")) {
        if (!store_open(argv[2], 64)) return 10;
        for (i = 0; i < 90; ++i) {
            p = store_get(i, &n);
            if (!p || n != 1000) return 11;
            for (j = 0; j < n; ++j)
                if (p[j] != (unsigned char)(i + j)) return 12;
        }
        before = store_reads();
        p = store_get(89, &n);
        if (!p || n != 1000 || store_reads() != before) return 13;
        p = store_get(0, &n);
        if (!p || n != 1000 || store_reads() != before + 1) return 14;
        for (j = 0; j < n; ++j)
            if (p[j] != (unsigned char)j) return 15;
        p = store_get(90, &n);
        if (!p || n != 70000) return 16;
        for (j = 0; j < n; ++j)
            if (p[j] != (unsigned char)j) return 17;
        store_close();
        return 0;
    }
    if (!store_open(argv[2], 64)) { puts(store_error()); store_close(); return 3; }
    p = store_get(UINT32_MAX, &n);
    if (p || n) return 4;
    p = store_get(1, &n);
    if (!p || n != 8 || memcmp(p, "\0\377binary", 8)) return 5;
    if (!store_check_readonly()) return 6;
    store_close();
    return 0;
}
'''

VFS_HARNESS = r'''
#include "sqlite3.h"
#include "dos_readonly_vfs.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    sqlite3_vfs *v;
    sqlite3_file *f;
    sqlite3 *db = 0;
    char bytes[8];
    int result = 0, rc;
    if (argc != 2 || sqlite3_initialize() != SQLITE_OK) return 1;
    v = sqlite3_vfs_find("dos-ro");
    if (!v) return 2;
    f = (sqlite3_file *)calloc(1, v->szOsFile);
    if (!f) return 3;
    if (v->xAccess(v, argv[1], SQLITE_ACCESS_EXISTS, &result) || !result) return 4;
    if (v->xAccess(v, "NO_SUCH_FILE.DB", SQLITE_ACCESS_EXISTS, &result) || result) return 5;
    rc = v->xOpen(v, argv[1], f, SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_READWRITE, 0);
    if (rc != SQLITE_READONLY || f->pMethods) return 6;
    rc = v->xOpen(v, argv[1], f, SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_READONLY, &result);
    if (rc || result != SQLITE_OPEN_READONLY) return 7;
    memset(bytes, 0x5a, sizeof(bytes));
    rc = f->pMethods->xRead(f, bytes, sizeof(bytes), LONG_MAX);
    if (rc != SQLITE_IOERR_SEEK) return 8;
    rc = f->pMethods->xRead(f, bytes, sizeof(bytes), 1000000);
    if (rc != SQLITE_IOERR_SHORT_READ || memcmp(bytes, "\0\0\0\0\0\0\0\0", 8)) return 9;
    if (f->pMethods->xWrite(f, bytes, sizeof(bytes), 0) != SQLITE_READONLY) return 10;
    if (f->pMethods->xTruncate(f, 0) != SQLITE_READONLY) return 11;
    if (f->pMethods->xClose(f)) return 12;
    if (v->xDelete(v, argv[1], 0) == SQLITE_OK) return 13;
    if (sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READWRITE, "dos-ro") == SQLITE_OK) return 14;
    sqlite3_close(db);
    free(f);
    return 0;
}
'''


def build():
    (WORK / "contract.c").write_text(HARNESS)
    (WORK / "vfs_contract.c").write_text(VFS_HARNESS)
    common = ["gcc", *FLAGS, *INCLUDES]
    run([*common, SOURCE / "bench.c", SOURCE / "pack_store.c",
         "-o", WORK / "pack_bench"])
    run([*common, SOURCE / "pack_store.c", WORK / "contract.c",
         "-o", WORK / "pack_contract"])
    run(["gcc", *FLAGS[:-3], "-w", *INCLUDES, *SQL_FLAGS,
         "-c", SQLITE / "sqlite3.c",
         "-o", WORK / "sqlite3.o"])
    sqlparts = [SOURCE / "sqlite_store.c", SOURCE / "dos_readonly_vfs.c",
                WORK / "sqlite3.o"]
    run([*common, *SQL_FLAGS, SOURCE / "bench.c", *sqlparts,
         "-o", WORK / "sql_bench"])
    run([*common, *SQL_FLAGS, WORK / "contract.c", *sqlparts,
         "-o", WORK / "sql_contract"])
    run([*common, *SQL_FLAGS, WORK / "vfs_contract.c",
         SOURCE / "dos_readonly_vfs.c", WORK / "sqlite3.o",
         "-o", WORK / "vfs_contract"])


def main():
    if not (SQLITE / "sqlite3.c").exists():
        raise SystemExit("SQLite amalgamation missing under .work/storage/sqlite")
    WORK.mkdir(parents=True, exist_ok=True)
    fixture()
    build()
    db_before = hashlib.sha256((WORK / "CONTENT.DB").read_bytes()).digest()
    pack_before = hashlib.sha256((WORK / "CONTENT.PAK").read_bytes()).digest()
    for backend, suffix in (("pack", "PAK"), ("sql", "DB")):
        output = run([WORK / (backend + "_bench"), WORK / ("CONTENT." + suffix), "64"])
        if "status=ok" not in output or "error=" in output:
            raise AssertionError(output)
        run([WORK / (backend + "_contract"), "lookup",
             WORK / ("CONTENT." + suffix), "unused"])
        run([WORK / (backend + "_contract"), "missing",
             WORK / "ABSENT.DB", "unused"])
    for bad in ("TRUNC.PAK", "BAD.PAK", "BAD_INDEX.PAK"):
        run([WORK / "pack_contract", "badfile", WORK / bad, "unused"])
    for bad in ("TRUNC.DB", "BAD.DB"):
        run([WORK / "sql_contract", "badfile", WORK / bad, "unused"])
    output = run([WORK / "pack_bench", WORK / "BAD_PAYLOAD.PAK", "64"], expected=3)
    if "error=payload_mismatch" not in output or "status=failed" not in output:
        raise AssertionError(output)
    run([WORK / "pack_contract", "stress", WORK / "STRESS.PAK", "unused"])
    run([WORK / "vfs_contract", WORK / "CONTENT.DB"])
    assert hashlib.sha256((WORK / "CONTENT.DB").read_bytes()).digest() == db_before
    assert hashlib.sha256((WORK / "CONTENT.PAK").read_bytes()).digest() == pack_before
    print("PASS: both traces, binary payloads, missing IDs/files, corrupt packs, "
          "VFS short read/write rejection, source bytes unchanged; ASan+UBSan")


if __name__ == "__main__":
    main()
