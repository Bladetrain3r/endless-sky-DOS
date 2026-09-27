# Content storage decision — 2026-09-27

**Use an indexed pack with bounded caching for the first DOS read-only content
store. Keep save persistence as a separate decision.** SQLite runs successfully
in our DOS environment; it is not ruled out by the platform. The measured workload
favors the smaller direct-index reader at comparable tracked buffer memory.

This answers one storage question, not the whole16MiB game challenge. These are
raw source blocks, not constructed ships, resolved missions or a running world.
Do not present the reader's buffer use as the entire game's new RAM requirement.

## Experiment and results

Both stores contain the same9,193 byte-preserved blocks from204 source files,
total10,500,260bytes. IDs are manifest positions for this pinned experiment, not
persistent game identifiers. Boundaries use unindented non-comment lines; this
is explicitly not a substitute for the game's semantic parser. Each source file
is exactly reconstructed from its chunks. Build-time verification compares every
SQLite row and pack payload against the original bytes, and runs SQLite integrity
checking. Runtime traces carry source-derived lengths and FNV checksums.

DOSBox0.74-3, normal/pentium_slow, fixed20,000cycles,16MB, CWSDPMI7 with swap
disabled. Both programs use the same driver, trace input and payload hash. All
queries pass. Times below include trace reads and payload checking; they are
batch totals, not frame times or simulated disk seek latency.

| Backend | Full scan9,193 | Scattered1,024 | Hot set1,024 | Tracked buffer/allocator peak |
| --- | ---: | ---: | ---: | ---: |
| Pack, uncached | 6.126s | 0.875s | 1.336s | 119.2KiB |
| Pack,64KiB cache | 6.473s | 0.917s | 0.865s | 219.8KiB |
| SQLite,128KiB cache setting | 8.938s | 1.477s | 1.227s | 226.8KiB |
| SQLite,32KiB cache setting | 8.917s | 2.019s | 2.669s | 185.1KiB |

Hot set is16 source records totaling33,103bytes. Cached pack makes16 backing
reads for those1,024 lookups; SQLite128 makes21VFS reads totaling86,016bytes.
With only32KiB cache, SQLite makes2,048VFS reads/8,388,608bytes in that phase.
The pack cache costs time when almost every record is read only once; use no
cache for streaming and a bounded cache for repeated content access. This simple
FIFO cache is not presented as an optimal replacement for SQLite's page manager.

Pack file10,573,820bytes; SQLite12,328,960bytes. DOS executables181,757bytes and
919,505bytes respectively, using size-optimized i386-targeted compilation without
MMX/SSE. Executable file sizes are not resident-code measurements. At these batch
sizes and this emulator setting, differences are useful; they are not a universal
SQLite benchmark and do not measure the value of SQL joins, transactions or tools.

The memory columns have different instrumentation: pack counts its explicit
index, payload and cache allocations; SQLite reports sqlite3_memory_highwater.
Neither includes libc/stdio/stack/code. DPMI free-page snapshots are also retained,
but are not high-water measurements (existing heap slack affects their deltas).
SQLite has a1MiB hard allocator limit. Database cache_size alone is not a whole
process memory cap. Host filesystem caches weren't flushed; real slow disks may
change the trade-off. No cold-disk or real-hardware claim follows.

## Reproduce

Prerequisite: run `python3 dos/tools/run_runtime.py` to establish the pinned DOS
toolchain. Then, from the checkout root:

```sh
python3 dos/storage/run_comparison.py
```

This verifies the versioned SQLite3.50.4 archive and its officially published
sqlite3.c SHA3, generates both stores in Docker, compiles them in Docker, executes
all four cases under a timeout and checks each result/trace count. All generated
inputs are rehashed after execution to detect modification. The one latest work
folder is `.work/storage/`; compact evidence is `dos/reports/storage-baseline.json`.
The SQLite engine is built single-threaded, OS_OTHER, without WAL/shared cache or
loadable extensions. Full compile flags are in build.sh. SQLite source is cached,
not copied into the repository; its exact URL/hashes are in the runner/report.
The database builder uses Python SQLite3.40.1; DOS reader uses3.50.4. No special
extension/schema feature is involved in this experiment.

The native contract tests are separate from timing. Run in the existing native
reference image with this checkout mounted at /work:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -e HOME=/tmp -v "$PWD:/work" -w /work \
  endless-sky-dos-native-reference:local python3 dos/storage/test_native.py
```

Native ASan+UBSan tests passed, independently rerun by root in Docker. Coverage
includes binary records, missing IDs/files, corrupt/truncated indexes/databases,
flipped payload detection, cache eviction/bypass, seek bounds, zero-filled short
reads and direct VFS write/RW-open refusals. LeakSanitizer is disabled after its
ptrace failure in the delegate environment; leak freedom is not established.

## DOS VFS boundary

`dos_readonly_vfs.c` provides SQLite's missing platform interface for this test.
It accepts only immutable, read-only main databases; rejects read/write opens,
create, writes, truncation, deletion and auxiliary files; zero-fills short reads;
checks seek bounds. Lock/unlock are no-ops justified only by the immutable/no-writer
contract. It must NOT be reused as a writable save backend. Save support needs
real write/flush/journal/recovery semantics even with a single application owner.
The mounted corpus must be finalized with no pending journal or WAL.

## Consequence for the port

Proceed with compiled content addressed by explicit IDs/offsets and bounded
residency. Keep active flight objects in RAM. Next gate is a typed, linked slice
of real systems/ship/outfit/mission data checked against upstream parsing, with
stable identities that survive builds and save/load. Avoid reconstructing every
world object merely because lookup became cheap. Use the database idea where its
features pay for themselves; this experiment doesn't qualify writable state.

References: [SQLite3.50.4 source hash](https://www.sqlite.org/releaselog/3_50_4.html),
[VFS contract](https://www.sqlite.org/c3ref/vfs.html),
[compile options](https://www.sqlite.org/compile.html).
