#!/usr/bin/env python3
"""Build a separate native world exporter and capture baseline PrintData oracles.

Run from any directory: python3 dos/world/export_native.py
The tracked source and .work/native/build/endless-sky are read-only inputs.
"""

import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / ".work/world"
IMAGE = "endless-sky-dos-native-reference:local"
FIXTURE = '''# Isolated resolved-world semantics probe; loaded after vanilla data.
system "DOS Fixture Alpha"
    pos 20000 20000
    habitable 320
    attributes first
    link "DOS Fixture Beta"
    link "DOS Fixture Blocked"
    object
        sprite star/g0
    object "DOS Fixture Shared"
        sprite planet/rock0
        distance 300
        object
            sprite planet/rock1
            distance 40
    trade Food 17

system "DOS Fixture Alpha"
    add attributes second
    remove attributes first
    remove link "DOS Fixture Blocked"
    add link "DOS Fixture Gamma"
    trade Food 0
    trade Clothing 100

system "DOS Fixture Beta"
    pos 20100 20000
    habitable 360
    link "DOS Fixture Alpha"
    link "DOS Fixture Blocked"
    object
        sprite star/m0
    object "DOS Fixture Shared"
        sprite planet/rock0
        distance 420

system "DOS Fixture Blocked"
    pos 20200 20000
    habitable 400
    inaccessible

system "DOS Fixture Gamma"
    pos 20000 20100
    habitable 420
    object "DOS Fixture Undefined"
        sprite planet/rock0
        distance 70

planet "DOS Fixture Shared"
    attributes initial
planet "DOS Fixture Shared"
    add attributes later
    remove attributes initial

system "DOS Fixture Reset"
    pos 20300 20000
    habitable 300
    trade Food 99
overwrite
system "DOS Fixture Reset"
    pos 20310 20000
    habitable 310
'''


def inside():
    world = Path("/work/world")
    source = Path("/src")
    build = Path("/baseline/build")
    stage = world / "native-src/source"
    shutil.copytree(source / "source", stage, dirs_exist_ok=True)
    header = (source / "source/System.h").read_text()
    anchor = "class System {\n"
    assert header.count(anchor) == 1
    header = header.replace("#include <optional>", "#include <map>\n#include <optional>")
    header = header.replace(anchor, anchor + "\tfriend std::map<std::string, int> ExportTrade(const System &system);\n")
    (stage / "System.h").write_text(header)

    object_path = world / "export.o"
    flags = [
        "-DAVIF_DLL", "-isystem", "/usr/include/SDL2", "-isystem", "/usr/include/AL",
        "-O3", "-DNDEBUG", "-std=c++20", "-flto=auto", "-fno-fat-lto-objects",
        "-Wall", "-pedantic-errors", "-Wold-style-cast", "-fno-rtti",
    ]
    subprocess.run([
        "/usr/bin/c++", *flags, "-I", str(stage),
        "-c", "/src/dos/world/export.cpp", "-o", str(object_path),
    ], check=True)

    command = shlex.split((build / "CMakeFiles/EndlessSky.dir/link.txt").read_text())
    main_object = "CMakeFiles/EndlessSky.dir/source/main.cpp.o"
    assert command.count(main_object) == 1
    command[command.index(main_object)] = str(object_path)
    # A friend declaration changes System's class definition. Recompile every
    # baseline TU whose dependency file includes System.h, so no linked TU has
    # the old definition (and leave all unaffected baseline objects reusable).
    affected = []
    for dependency in sorted(build.rglob("*.o.d")):
        if b"/src/source/System.h" not in dependency.read_bytes():
            continue
        original = dependency.with_suffix("")
        relative = original.relative_to(build).as_posix()
        if relative == main_object:
            continue
        source_part = relative.split(".dir/", 1)[1].removesuffix(".o")
        staged_source = stage / source_part
        staged_object = world / "objects" / relative
        staged_object.parent.mkdir(parents=True, exist_ok=True)
        affected.append((relative, staged_source, staged_object))

    def compile_one(item):
        _, staged_source, staged_object = item
        subprocess.run(["/usr/bin/c++", *flags, "-I", str(stage), "-c",
                        str(staged_source), "-o", str(staged_object)], check=True)

    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(compile_one, affected))
    for relative, _, staged_object in affected:
        assert command.count(relative) == 1, relative
        command[command.index(relative)] = str(staged_object)
    output = command.index("-o") + 1
    assert command[output] == "endless-sky"
    command[output] = str(world / "export-native")
    subprocess.run(command, cwd=build, check=True)

    config = world / "empty-config"
    if config.exists():
        shutil.rmtree(config)
    config.mkdir(exist_ok=True)
    args = ["--resources", "/src", "--config", str(config), "--tq-threads", "1"]
    outputs = {
        "native.json": [str(world / "export-native"), *args],
        "systems.txt": [str(build / "endless-sky"), *args, "--systems"],
        "systems-attributes.txt": [str(build / "endless-sky"), *args, "--systems", "--attributes"],
        "planets.txt": [str(build / "endless-sky"), *args, "--planets"],
        "planets-attributes.txt": [str(build / "endless-sky"), *args, "--planets", "--attributes"],
        "orbits-all.txt": [str(build / "endless-sky"), *args, "--orbits", "--all"],
        "orbits-sol.txt": [str(build / "endless-sky"), *args, "--orbits", "--systems", "Sol"],
    }
    record = {}
    for name, command in outputs.items():
        result = subprocess.run(command, cwd=source, capture_output=True, timeout=120)
        (world / name).write_bytes(result.stdout)
        (world / (name + ".stderr")).write_bytes(result.stderr)
        if result.returncode and name != "orbits-all.txt":
            raise RuntimeError(f"{name}: exit {result.returncode}: {result.stderr[-1500:]!r}")
        record[name] = {"command": command, "sha256": hashlib.sha256(result.stdout).hexdigest(),
                        "bytes": len(result.stdout), "returncode": result.returncode}
    world_data = json.loads((world / "native.json").read_text())
    assert world_data["schema"] == 1
    record["counts"] = {key: len(world_data[key]) for key in ("systems", "planets", "commodities")}
    record["baseline_binary_sha256"] = hashlib.sha256((build / "endless-sky").read_bytes()).hexdigest()
    record["source_commit"] = (Path("/baseline/source-commit.txt")).read_text().strip()
    record["docker_image_id"] = os.environ["WORLD_IMAGE_ID"]
    record["source_file_sha256"] = hashlib.sha256((source / "dos/world/export.cpp").read_bytes()).hexdigest()
    record["runner_file_sha256"] = hashlib.sha256((source / "dos/world/export_native.py").read_bytes()).hexdigest()
    record["baseline_system_header_sha256"] = hashlib.sha256((source / "source/System.h").read_bytes()).hexdigest()
    record["staged_system_header_sha256"] = hashlib.sha256((stage / "System.h").read_bytes()).hexdigest()
    record["export_binary_sha256"] = hashlib.sha256((world / "export-native").read_bytes()).hexdigest()
    resource_digest = hashlib.sha256()
    resource_count = 0
    for path in sorted((source / "data").rglob("*")):
        if path.is_file():
            relative = path.relative_to(source).as_posix().encode()
            resource_digest.update(len(relative).to_bytes(4, "little"))
            resource_digest.update(relative)
            resource_digest.update(hashlib.sha256(path.read_bytes()).digest())
            resource_count += 1
    record["resource_data_sha256"] = resource_digest.hexdigest()
    record["resource_data_files"] = resource_count
    record["plugins"] = []
    record["recompiled_system_users"] = len(affected)
    fixture_config = world / "fixture-config"
    if fixture_config.exists():
        shutil.rmtree(fixture_config)
    fixture_data = fixture_config / "plugins/world-fixture/data"
    fixture_data.mkdir(parents=True)
    fixture_file = fixture_data / "world-fixture.txt"
    fixture_file.write_text(FIXTURE)
    fixture_args = ["--resources", "/src", "--config", str(fixture_config), "--tq-threads", "1"]
    fixture_outputs = {
        "fixture-native.json": [str(world / "export-native"), *fixture_args],
        "fixture-orbits-alpha.txt": [str(build / "endless-sky"), *fixture_args, "--orbits", "--systems", "DOS Fixture Alpha"],
        "fixture-orbits-beta.txt": [str(build / "endless-sky"), *fixture_args, "--orbits", "--systems", "DOS Fixture Beta"],
    }
    for name, command in fixture_outputs.items():
        result = subprocess.run(command, cwd=source, capture_output=True, timeout=120)
        (world / name).write_bytes(result.stdout)
        (world / (name + ".stderr")).write_bytes(result.stderr)
        if result.returncode:
            raise RuntimeError(f"{name}: exit {result.returncode}: {result.stderr[-1500:]!r}")
        record[name] = {"command": command, "sha256": hashlib.sha256(result.stdout).hexdigest(),
                        "bytes": len(result.stdout), "returncode": result.returncode}
    fixture_world = json.loads((world / "fixture-native.json").read_text())
    by_system = {row["name"]: row for row in fixture_world["systems"]}
    by_planet = {row["name"]: row for row in fixture_world["planets"]}
    alpha = by_system["DOS Fixture Alpha"]
    beta = by_system["DOS Fixture Beta"]
    assert alpha["valid"] and alpha["links"] == ["DOS Fixture Beta", "DOS Fixture Gamma"]
    assert alpha["attributes"] == ["second", "uninhabited"]
    assert alpha["trade"] == [{"commodity": "Clothing", "price": 100}, {"commodity": "Food", "price": 0}]
    assert [(item["index"], item["parent"]) for item in alpha["objects"]] == [(0, -1), (1, -1), (2, 1)]
    assert beta["links"] == ["DOS Fixture Alpha"]
    assert by_planet["DOS Fixture Shared"]["systems"] == ["DOS Fixture Alpha", "DOS Fixture Beta"]
    assert by_planet["DOS Fixture Shared"]["attributes"] == ["later"]
    assert not by_planet["DOS Fixture Undefined"]["valid"]
    assert by_system["DOS Fixture Gamma"]["objects"][0]["planet"] == "DOS Fixture Undefined"
    assert by_system["DOS Fixture Reset"]["trade"] == []
    record["fixture_assertions"] = "passed"
    record["fixture_sha256"] = hashlib.sha256(FIXTURE.encode()).hexdigest()
    (world / "provenance.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record["counts"]))


def outside():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline_commit = (ROOT / ".work/native/source-commit.txt").read_text().strip()
    guarded = ["source", "CMakeLists.txt", "cmake"]
    subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", baseline_commit,
                    "--", *guarded], check=True)
    subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", "--",
                    *guarded], check=True)
    subprocess.run(["git", "-C", str(ROOT), "diff", "--cached", "--quiet", "--",
                    *guarded], check=True)
    unknown = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "--others",
                                       "--exclude-standard", "--", *guarded], text=True)
    if unknown:
        raise RuntimeError("Untracked native source/build inputs differ from the baseline; rebuild the native reference")
    image_id = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE], text=True).strip()
    subprocess.run([
        "docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
        "--env", "HOME=/tmp", "--env", f"WORLD_IMAGE_ID={image_id}",
        "--mount", f"type=bind,src={ROOT},dst=/src,readonly",
        "--mount", f"type=bind,src={ROOT / '.work/native'},dst=/baseline,readonly",
        "--mount", f"type=bind,src={WORK},dst=/work/world", IMAGE,
        "python3", "/src/dos/world/export_native.py", "--inside",
    ], check=True)


if __name__ == "__main__":
    try:
        inside() if sys.argv[1:] == ["--inside"] else outside()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as exc:
        sys.exit(f"world export failed: {exc}")
