# Endless Sky DOS investigation

The user brief is [Wishlist.md](../Wishlist.md), retained unchanged. The target
is a reasonably complete vanilla experience on DOS, 16 MB and fixed 20,000
DOSBox cycles, preferably 800x600 at 30 FPS. This directory starts the port's
measurement and planning record. **There is no playable DOS port yet.**

Current upstream checkout: `061a9461a93898fb691504536589d1dcddc5d79b`
(CMake project version 0.11.4). See [feasibility](FEASIBILITY.md) and
[handover](HANDOVER.md). Source/backend work should retain upstream notices and
GPL-3.0-or-later licensing; asset conversions retain the per-file provenance and
terms recorded in `../copyright`.

## First executable: banked VBE diagnostic

`probes/vbe_fill.asm` is a standalone NASM DOS COM program. It queries mode 103h,
requires a writable window A with compatible bank geometry and 800x600x8 layout,
writes 1,200 frames, records BIOS ticks, verifies endpoint samples from all eight
banks, restores text mode, and writes its report to stdout. It does not contain
Endless Sky simulation or assets. Colors differ by bank to detect aliased writes.

The default runner reuses the existing local `modern-arena-tools:0.1` Docker image:

```sh
python3 dos/tools/run_probe.py
```

For a separate small tools image, from the checkout root:

```sh
docker build -f dos/Dockerfile.probe -t endless-sky-dos-probe:local dos
DOS_PROBE_IMAGE=endless-sky-dos-probe:local python3 dos/tools/run_probe.py
```

The standalone Dockerfile is supplied but has not yet been built here. Its base
and apt packages float; the runner records the actual image ID and package
versions. For a release, pin the chosen toolchain and emulator inputs. The saved
baseline used the existing image whose exact ID is in the report.

Execution is network-disabled in a disposable container, user-owned output,
Xvfb, 90-second emulator timeout. Configuration: DOSBox 0.74-3, normal core,
pentium_slow, svga_s3, 16 MB, fixed 20,000 cycles, no sound or frame skipping.
`reports/vbe-fill-baseline.json` is the compact evidence. `.work/dos-probe/`
contains only the latest binary, emulator log and raw output; reruns replace them.
The BIOS timer gives a batch average, not frame-time percentiles. The runner
rejects missing success/sample markers or invalid timing; it does not trust the
emulator's exit status alone as a guest-program verdict.

## Protected-mode C++ runtime qualification

```sh
python3 dos/tools/run_runtime.py
```

Downloads versioned DJGPP12.2 and CWSDPMI7 archives, verifies recorded SHA256s,
compiles/runs inside Docker. Runtime configuration starts `CWSDPMI -s-` to disable
swap. GNU C++20 is used for this platform probe because strict ANSI mode hides
the DJGPP DPMI declarations. Compilation requests i386 instructions and disables
MMX/SSE; bundled libraries have not received an exhaustive instruction audit.

`reports/runtime-baseline.json`:602,453-byte executable; concepts/span/vector/map/
string/exceptions pass;4MiB allocation touched and checked; largest available
block before that allocation14,663,680bytes. These are runtime observations, not
a measured game memory budget. Cached compiler+archives occupy about327MiB; this
is reusable build tooling, not copied for each experiment. No swap file observed.
CWSDPMI provenance and redistribution terms are in the downloaded archive
`bin/cwsdpmi.doc`; preserve them and its source-availability notice in any package.

## Native reference

```sh
bash dos/tools/native_reference.sh
```

Docker builds upstream unchanged, with source read-only and two compile jobs;
it then measures a data-only load and one seeded headless landing test. Results
are retained in `reports/native-baseline.json`:95,088KiB and437,932KiB peak native
RSS respectively. Both pass; the landing wrapper checks a completion marker.
The script uses `--rngseed` because upstream's help spelling is incorrect here.
Native measurements are not DOS memory requirements. Audio is not validated.
The image build installs network packages; subsequent build/tests are isolated
with network disabled, temporary configs and timeouts. Cached build is85MiB.

## Content-store comparison

[Storage decision and reproduction](storage/README.md): both SQLite and an
indexed pack run in DOS against the same9,193 source blocks. All payload checks
pass. At similar tracked memory (~220 vs227KiB), the cached pack was faster for
scan, scattered and repeated-working-set reads in this experiment. Select the
pack for initial read-only runtime content; this is not a decision against SQLite
for build tools or qualified transactional saves. No writable DOS VFS exists yet.

## Next executable milestone

Initial DOS graphics, C++/DPMI and content-store probes pass; the unmodified native
reference builds/tests. Next: compile a typed, linked data slice with stable IDs
and paged text, checked against upstream semantics. Then software menu/flight
with actual preprocessed assets and separate render/sim measurements. See
FEASIBILITY.md and EQUIVALENCE.md. Low-level implementation choices are delegated.
