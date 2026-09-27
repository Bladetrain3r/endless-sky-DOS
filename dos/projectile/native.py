#!/usr/bin/env python3
"""Build unchanged-upstream Projectile constructor/Move fixtures in Docker.

Copyright (c) 2026 Endless Sky DOS contributors.
SPDX-License-Identifier: GPL-3.0-or-later
"""

import csv
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / ".work/projectile"
IMAGE = "endless-sky-dos-native-reference:local"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside():
    work = Path("/work/projectile")
    build = Path("/baseline/build")
    source = Path("/src/dos/projectile/oracle.cpp")
    obj, binary = work / "native-oracle.o", work / "native-oracle"
    flags = ["-DAVIF_DLL", "-isystem", "/usr/include/SDL2", "-isystem", "/usr/include/AL",
             "-O3", "-DNDEBUG", "-std=c++20", "-flto=auto", "-fno-fat-lto-objects",
             "-Wall", "-pedantic-errors", "-Wold-style-cast", "-fno-rtti"]
    subprocess.run(["/usr/bin/c++", *flags, "-I", "/src/source", "-c", str(source),
                    "-o", str(obj)], check=True)
    link = shlex.split((build / "CMakeFiles/EndlessSky.dir/link.txt").read_text())
    main = "CMakeFiles/EndlessSky.dir/source/main.cpp.o"
    assert link.count(main) == 1
    link[link.index(main)] = str(obj)
    assert link[link.index("-o") + 1] == "endless-sky"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    config = work / "native-empty-config"
    if config.exists():
        shutil.rmtree(config)
    config.mkdir()
    csv_path = work / "native.csv"
    result = subprocess.run([str(binary), str(csv_path), "--resources", "/src",
                             "--config", str(config), "--tq-threads", "1"],
                            cwd="/src", capture_output=True, text=True, timeout=180)
    (work / "native.stdout").write_text(result.stdout)
    (work / "native.stderr").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"oracle exit {result.returncode}: {result.stderr[-1500:]}")
    stats = dict(item.split("=", 1) for item in result.stdout.strip().split())
    assert stats["cases"] == "12" and stats["rows"] == "600"
    assert stats["weapon"] == "Energy_Blaster"
    assert stats["lifetime"] == "48" and stats["velocity"] == "10.625"
    assert stats["random_lifetime"] == stats["random_velocity"] == "0"
    with csv_path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 600
    cases = {}
    for row in rows:
        cases.setdefault(row["case"], []).append(row)
    assert len(cases) == 12
    for name, trace in cases.items():
        assert [int(row["frame"]) for row in trace] == list(range(50)), name
        for frame, row in enumerate(trace):
            assert int(row["is_dead"]) == int(frame >= 48), (name, frame)
            assert int(row["is_removed"]) == int(frame >= 48), (name, frame)
            assert int(row["angle_steps"]) == int(row["launch_angle_steps"]), (name, frame)
            assert int(row["spawned_projectiles"]) == 0, (name, frame)
    record = {
        "schema": 1,
        "source_commit": Path("/baseline/source-commit.txt").read_text().strip(),
        "image_id": os.environ["PROJECTILE_IMAGE_ID"],
        "baseline_binary_sha256": digest(build / "endless-sky"),
        "oracle_source_sha256": digest(source),
        "runner_source_sha256": digest(Path("/src/dos/projectile/native.py")),
        "oracle_binary_sha256": digest(binary),
        "csv_sha256": digest(csv_path),
        "weapon_data_sha256": digest(Path("/src/data/human/weapons.txt")),
        "behavior_source_sha256": {name: digest(Path("/src/source") / name)
                                   for name in ("Projectile.cpp", "Projectile.h", "Engine.cpp",
                                                "Hardpoint.cpp", "CollisionSet.cpp")},
        "compiler_flags": flags,
        "rows": len(rows), "cases": sorted(cases), "frames_per_case": 50,
        "weapon": {key: float(value) for key, value in stats.items()
                   if key not in ("cases", "rows", "weapon")},
        "fields": list(rows[0]),
        "interface": "frame 0 is original Projectile(parent, explicit launch Point, explicit Angle, Energy Blaster); frames 1..49 are direct original Projectile::Move calls. Engine prunes after frame 48; frame 49 is a diagnostic direct call, not reachable engine behavior.",
        "fixture_scope": "No Weapon firing/hardpoint or inaccuracy applied. Parent is a Sparrow copied from loaded GameData and placed at (500,-250) with case velocity/heading; launch position/heading supplied explicitly. No target. Case data is in each CSV row.",
        "engine_order": "Engine::MoveShip calls Ship::Fire into newProjectiles while moving ships; Engine::CalculateUnpaused then moves existing projectiles, prunes removed, appends newProjectiles, fills collision sets, and checks collision for all projectiles. Thus a new shot can collide on its launch frame before its first Move. See source/Engine.cpp MoveShip and CalculateUnpaused.",
    }
    (work / "native-provenance.json").write_text(json.dumps(record, indent=2) + "\n")
    print(result.stdout.strip())


def outside():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline = (ROOT / ".work/native/source-commit.txt").read_text().strip()
    guarded = ["source", "CMakeLists.txt", "cmake", "data"]
    for extra in ([baseline, "--"], ["--"], ["--cached", "--"]):
        subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", *extra, *guarded], check=True)
    unknown = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "--others",
                                       "--exclude-standard", "--", *guarded], text=True)
    if unknown:
        raise RuntimeError("Untracked upstream source/data input; rebuild native reference")
    image_id = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE],
                                       text=True).strip()
    subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                    "--env", "HOME=/tmp", "--env", f"PROJECTILE_IMAGE_ID={image_id}",
                    "--mount", f"type=bind,src={ROOT},dst=/src,readonly",
                    "--mount", f"type=bind,src={ROOT / '.work/native'},dst=/baseline,readonly",
                    "--mount", f"type=bind,src={WORK},dst=/work/projectile", IMAGE,
                    "python3", "/src/dos/projectile/native.py", "--inside"], check=True)


if __name__ == "__main__":
    try:
        inside() if sys.argv[1:] == ["--inside"] else outside()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(f"projectile oracle failed: {error}")
