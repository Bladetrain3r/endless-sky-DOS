# Typed world snapshot

First immutable starting-world snapshot and DOS validator, **not a playable
simulation**. Native parsing resolves definitions and references at build time.
Ships, outfits, missions, services, evolving trade and event-driven changes are
outside this slice. [Exporter details](EXPORT.md) describe the consistent staged
native build; tracked upstream source and baseline executable stay untouched.

## Measured checkpoint

Vanilla:694 systems,619 planets,10 commodities,1,612 accessible links,5,518 orbital
objects,4,800 present trade prices and629 planet/system memberships. Pack size:
648,042 bytes. Resident directory:31,752 bytes; maximum record scratch:5,758 bytes.
Total explicit heap requests:37,510 bytes, below the256KiB access-layer ceiling.
This validator does not allocate a decoded live-object graph.

DOSBox normal/pentium_slow/fixed20,000 cycles,16MiB, swap disabled: about1.13s to
validate the whole pack. This includes sequential I/O, checksum and reference
checks; it is not game FPS, random-access timing or a full-game memory result.
Executable205,749 bytes. Heap accounting excludes code, runtime, stdio and stack;
DPMI page samples before/after are not peak-memory measurements.
[Recorded DOS results](../reports/world-baseline.json).

Every record independently decodes back to its native-export row with exact
fields and resolved references. Original native system/planet names and
attributes match all rows. Sol's19 objects and fixture Alpha/Beta's3/2 objects
match the native printer at its six-significant-digit precision. Upstream's
`--orbits --all` crashes: partial output is retained as failure evidence, never
counted as a passing oracle. [Native evidence](../reports/world-native-baseline.json).

ASan/UBSan gates reject13 corrupted packs; builder rejects5 invalid inputs.
Fixtures exercise duplicate IDs, offsets/lengths, truncation, checksums and a
wrong-kind reference whose checksum was repaired. DOS independently rejects
truncation and wrong-kind fixtures. Native loader fixtures cover forward refs,
add/remove, overwrite, inaccessible links, nested orbits, multi-system planets,
undefined placeholders and present zero-price trade. Their full snapshot also
passes binary roundtrip and the sanitized C reader. No complete gameplay parity
claim follows from these checks.

## Reproduction

Prerequisites: existing native reference image/build from
`dos/tools/native_reference.sh`, hash-pinned DJGPP/CWSDPMI from
`dos/tools/run_runtime.py`. From the checkout root:

```sh
python3 dos/world/export_native.py
mkdir -p .work/world/run
docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/work" -w /work endless-sky-dos-native-reference:local \
  python3 dos/world/pack.py .work/world/native.json .work/world/run/WORLD.PAK
docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/work" -w /work endless-sky-dos-native-reference:local \
  python3 dos/world/test_pack.py
docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/work" -w /work endless-sky-dos-native-reference:local \
  python3 dos/world/check_oracles.py
python3 dos/world/run_dos.py
```

## Format1 contract

All integers little-endian; doubles IEEE754 binary64. Header24bytes: `ESWORLD1`,
then uint32 record count,maximum record length,payload start,exact file size.
Directory entries24bytes: uint64 ID,uint32 kind,offset,length,FNV-1a32 checksum.
IDs sort ascending; payloads follow contiguously in directory order, exact EOF.

ID = first8bytes of SHA256 interpreted little-endian, hashing `ESWORLD1\0` +
kind(uint32LE) + UTF8-name byte length(uint32LE) + exact name bytes. Kinds1/2/3
are system/planet/commodity. Reject zero and collisions. Reordering or adding
unrelated records preserves IDs; renaming does not. Future saved references
should retain names and content hashes. Object indices are snapshot-local.

Strings: uint32 byte length + UTF8 without NUL; nullable sprite alone allows
length0xffffffff. Each payload begins with registry-name string, then:

- System: display string,flags,8 doubles,attributes,link IDs,objects,trade pairs.
- Planet: display string,flags,attributes,system membership IDs.
- Commodity: int32 ordinal,low,high.

Arrays have uint32 counts. Attributes are inline strings in this first format.
System flag bits0–3:valid/hidden/shrouded/inaccessible; planet0–1:valid/inhabited.
System doubles:x,y,jump range,hyper arrival,jump arrival,hyper departure,jump
 departure,habitable zone. Objects:int32 index,parent; uint64 planet ID(0null);
3 doubles distance,period,offset; nullable sprite string. Trade:uint64 commodity
ID + int32 price. Absent prices stay absent. Preserve invalid placeholders;
accessible route edges differ from raw declarations. Dynamic changes need a
mutable overlay or rebuilt records, not a permanently frozen universe.

Limits:8,192 records,32KiB record,256KiB directory+scratch. Reuse one buffer;
future APIs must not retain pointers into it. Check bounds, UTF8, finite doubles,
ordered IDs, checksums, references/kinds, duplicate links and parent order.
FNV detects accidental corruption, not authentication; SHA256 manifests identify
artifacts. [Owned active-system loading](ACTIVE.md) and the
[first DOS flight scene](../flight/README.md) are now implemented.
