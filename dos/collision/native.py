#!/usr/bin/env python3
"""Build an unchanged-upstream Mask::Create/Collide/Contains reference oracle."""

import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / ".work/collision"
IMAGE = "endless-sky-dos-native-reference:local"
NAMES = ("sparrow", "star barge", "falcon")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside():
    work = Path("/work/collision")
    build = Path("/baseline/build")
    source = Path("/src/dos/collision/oracle.cpp")
    obj = work / "oracle.o"
    binary = work / "oracle-native"
    flags = ["-DAVIF_DLL", "-isystem", "/usr/include/SDL2", "-isystem", "/usr/include/AL",
             "-O3", "-DNDEBUG", "-std=c++20", "-flto=auto", "-fno-fat-lto-objects",
             "-Wall", "-pedantic-errors", "-Wold-style-cast", "-fno-rtti"]
    subprocess.run(["/usr/bin/c++", *flags, "-I", "/src/source", "-c", str(source),
                    "-o", str(obj)], check=True)
    link = shlex.split((build / "CMakeFiles/EndlessSky.dir/link.txt").read_text())
    main_obj = "CMakeFiles/EndlessSky.dir/source/main.cpp.o"
    assert link.count(main_obj) == 1
    link[link.index(main_obj)] = str(obj)
    assert link[link.index("-o") + 1] == "endless-sky"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    masks = work / "masks.txt"
    tests = work / "tests.tsv"
    result = subprocess.run([str(binary), str(masks), str(tests)], cwd="/src",
                            capture_output=True, text=True, timeout=180)
    (work / "native.stdout").write_text(result.stdout)
    (work / "native.stderr").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"oracle exit {result.returncode}: {result.stderr[-1500:]}")
    with tests.open(newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    required = {"mask_id", "angle_steps", "sx", "sy", "vx", "vy", "contains",
                "collision_fraction"}
    assert rows and set(rows[0]) == required
    counts = [0, 0, 0]
    outcomes = {"contains": 0, "miss": 0, "hit": 0, "starts_inside": 0, "zero_vector": 0}
    for row in rows:
        mask_id = int(row["mask_id"])
        angle = int(row["angle_steps"])
        contains = int(row["contains"])
        fraction = float(row["collision_fraction"])
        assert 0 <= mask_id < 3 and 0 <= angle < 65536
        assert contains in (0, 1) and 0 <= fraction <= 1
        counts[mask_id] += 1
        outcomes["contains"] += contains
        outcomes["miss"] += fraction == 1
        outcomes["hit"] += 0 < fraction < 1
        outcomes["starts_inside"] += fraction == 0
        outcomes["zero_vector"] += float(row["vx"]) == float(row["vy"]) == 0
    assert min(counts) >= 2000 and all(outcomes.values())
    with masks.open() as stream:
        header = stream.readline().split()
        assert header == ["ESMASK1", "3"]
        geometry = []
        for mask_id in range(3):
            fields = stream.readline().split()
            assert len(fields) == 4 and int(fields[0]) == mask_id
            outlines, total, radius = int(fields[1]), int(fields[2]), float(fields[3])
            assert outlines > 0 and total >= outlines * 3 and radius > 0
            actual = 0
            for _ in range(outlines):
                count = int(stream.readline())
                assert count >= 3
                for _ in range(count):
                    x, y = map(float, stream.readline().split())
                    assert abs(x) <= radius and abs(y) <= radius
                actual += count
            assert actual == total
            geometry.append({"id": mask_id, "name": NAMES[mask_id], "outlines": outlines,
                             "points": total, "radius": radius})
        assert not stream.read().strip()
    for line, item in zip(result.stdout.strip().splitlines()[:3], geometry):
        assert line.startswith(f"{item['id']} {item['name']} ")
        dimensions = re.search(r"\b\d+x\d+\b", line)
        assert dimensions
        item["image_dimensions"] = dimensions.group()
    assert result.stdout.strip().splitlines()[-1] == f"cases={len(rows)}"
    record = {
        "schema": 1,
        "source_commit": Path("/baseline/source-commit.txt").read_text().strip(),
        "image_id": os.environ["COLLISION_IMAGE_ID"],
        "baseline_binary_sha256": digest(build / "endless-sky"),
        "oracle_source_sha256": digest(source),
        "runner_source_sha256": digest(Path("/src/dos/collision/native.py")),
        "oracle_binary_sha256": digest(binary),
        "masks_sha256": digest(masks), "tests_sha256": digest(tests),
        "original_images_sha256": {name: digest(Path("/src/images/ship") / f"{name}.png")
                                   for name in NAMES},
        "compiler_flags": flags, "rows": len(rows), "per_mask_rows": counts,
        "outcomes": outcomes, "geometry": geometry,
        "mask_coordinates": "Original Mask::Create, frame 0 of original 1x PNG, premultiplied via ImageBuffer::Read; SmoothAndCenter yields 0.5x sprite coordinates around image center",
        "test_schedule": "Per mask: twelve fixed headings with zero/start-inside/miss/lines and seeded random; every exported vertex and edge midpoint including zero vector and rotated cases; 2400 seeded arbitrary-angle lines",
        "fields": "mask_id,angle_steps,sx,sy,vx,vy,contains,collision_fraction",
        "seed": "0x5a17d3eab921460f, xorshift64star; deterministic generation order",
    }
    (work / "provenance.json").write_text(json.dumps(record, indent=2) + "\n")
    print(result.stdout.strip())


def outside():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline = (ROOT / ".work/native/source-commit.txt").read_text().strip()
    guarded = ["source", "CMakeLists.txt", "cmake", "images/ship/sparrow.png",
               "images/ship/star barge.png", "images/ship/falcon.png"]
    for extra in ([baseline, "--"], ["--"], ["--cached", "--"]):
        subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", *extra, *guarded], check=True)
    unknown = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "--others",
                                       "--exclude-standard", "--", *guarded], text=True)
    if unknown:
        raise RuntimeError("Untracked upstream source/image input; rebuild native reference")
    image_id = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE],
                                       text=True).strip()
    subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                    "--env", "HOME=/tmp", "--env", f"COLLISION_IMAGE_ID={image_id}",
                    "--mount", f"type=bind,src={ROOT},dst=/src,readonly",
                    "--mount", f"type=bind,src={ROOT / '.work/native'},dst=/baseline,readonly",
                    "--mount", f"type=bind,src={WORK},dst=/work/collision", IMAGE,
                    "python3", "/src/dos/collision/native.py", "--inside"], check=True)


if __name__ == "__main__":
    try:
        inside() if sys.argv[1:] == ["--inside"] else outside()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(f"collision oracle failed: {error}")
