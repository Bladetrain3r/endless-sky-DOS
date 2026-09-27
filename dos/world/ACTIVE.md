# Bounded active-system loader

`active.h` and `active.c` open the immutable ESWORLD1 pack and decode one system
at a time. The `WorldStore` owns the directory, one record scratch buffer and an
open file. `world_open` checks the exact file length, sorted IDs, contiguous
payload spans, kind limits, every record checksum, UTF-8 string, finite double,
array bound, reference kind/existence, duplicate link/trade IDs and orbit parent
order before returning. `world_load_system` checks its record again because the
file can change after opening.

`world_find_system` resolves a registry name to an ID. Its output ID is unchanged
on failure. `world_load_system` requires a zero-initialized `ActiveSystem`; it
rejects a populated output, and any failure leaves it untouched. Release with
`world_release_system`, which clears the struct. A loaded system has one owned
allocation for its arrays and NUL-terminated name/display/sprite strings. No
pointer in it refers to the store scratch buffer, so it remains valid across
other loads and after `world_close`. Nullable sprite is `NULL`. Attributes are
validated but not exposed. Planet and commodity references remain stable IDs.
This is the immutable starting snapshot, not a mutable gameplay world.

The store's own explicit heap (`world_store_bytes`) includes its struct, directory
and scratch, capped at256KiB (37,558 bytes in the native64-bit test). Each active allocation
is capped at64KiB; the largest vanilla system uses6,096 bytes in that test.
DOS structure sizes differ; the flight report measures actual DOS residency. A record that needs more fails rather than truncating. Stdio, runtime, stack and
allocator metadata are outside that accounting. The API is single-threaded
because one scratch buffer and file handle are shared.

Run the native integration gate from the checkout root:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/work" -w /work endless-sky-dos-native-reference:local \
  python3 dos/world/test_active.py
```

The same gate passed in `endless-sky-dos-native-reference:local` with ASan/UBSan.
It compares every exposed Sol and Sirius scalar, object, link and price against
`native.json`, holds Sol while loading Sirius, verifies ownership after store close, reloads each tested system100 times, loads all
694 systems in one store, and rejects
truncation, checksum damage, invalid header counts/offsets and a repaired-checksum
wrong-kind reference. The source also compiled with cached DJGPP 12.2 using the
project's i386/no-MMX/no-SSE flags. The DOS harness ran with CWSDPMI swap disabled
in headless DOSBox against `.work/world/run/WORLD.PAK`; its Sol fields and first
objects match native output. The runtime test was a functional smoke test, not a
peak memory or frame-rate measurement.
