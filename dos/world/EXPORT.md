# Resolved native world export

Run `python3 dos/world/export_native.py` from the checkout root or another
directory. It uses the existing `endless-sky-dos-native-reference:local` Docker
image and `.work/native/build` (the unchanged Release baseline). Docker runs
without network access. The script leaves tracked upstream source and the
baseline executable unchanged.

The exporter links the baseline's native object files, replacing `main.cpp.o`
with `dos/world/export.cpp`. To read **actual** initial trade map entries,
the runner stages `source/System.h` with one `ExportTrade` friend declaration.
It copies native source into `.work/world/native-src/source`, recompiles all
objects whose baseline dependency files include `System.h`, and substitutes
those objects at link time. The rest of the baseline objects are reused. This
keeps one consistent `System` class definition among linked translation units.
The staged declaration changes no class layout or game behavior. No exporter
accessor is added to tracked upstream code.

`native.json` in `.work/world/` is schema 1. It is captured after the normal
native data load and `UniverseObjects::FinishLoading`, which the loader invokes
internally. Both valid and placeholder registry rows are retained. Pointer
references resolve through registry keys, including undefined placeholders.
Pointer-based link and membership collections are sorted by key. Doubles use
`max_digits10`, with signed zero preserved; JSON strings escape control characters. Trade includes every
entry actually present in the native map, including zero prices, and no inferred
entries.

`provenance.json` records the source commit, data-tree digest, baseline binary
digest, staged header and exporter hashes, Docker image ID, count and hash of
each output, exact in-container command, plugin list,
and count of rebuilt `System.h` users. The script starts from an empty config;
vanilla outputs use no plugins. Native baseline `--systems`, `--systems
--attributes`, `--planets`, `--planets --attributes`, and orbit probes are saved
beside the JSON. The unchanged baseline reproducibly exits 139 on `--orbits
--all` after emitting about 64 KiB; its partial output, stderr and exit code
are retained. `--orbits --systems Sol` succeeds. The crash limits the complete
independent orbit oracle; it does not affect the exporter.

The runner also generates an isolated `fixture-config` plugin and captures
`fixture-native.json` plus native orbit probes for its Alpha and Beta systems.
Assertions cover forward references resolved by later definitions, repeated
system and planet definitions, attribute add/remove, inaccessible-link
filtering, a nested orbit, multi-system planet membership, repeated trade with
an actual zero entry, and top-level overwrite clearing old trade. The fixture
is outside the vanilla config and its text hash appears in provenance. Before
linking against baseline objects, the runner checks tracked native source and
CMake inputs against the baseline source commit and rejects local drift.

The output is a read-only starting snapshot. It does not include dynamic events,
economy steps, services, or a DOS runtime. Source and data provenance are
specific to the recorded checkout and resource tree.
