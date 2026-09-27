#!/usr/bin/env python3
"""Build and run the unchanged-source native Ship::Move reference oracle."""

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
WORK = ROOT / ".work/motion"
IMAGE = "endless-sky-dos-native-reference:local"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inside():
    work = Path("/work/motion")
    build = Path("/baseline/build")
    source = Path("/src/dos/motion/oracle.cpp")
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
    csv_path = work / "native.csv"
    angle_path = work / "angles.bin"
    config = work / "empty-config"
    if config.exists():
        shutil.rmtree(config)
    config.mkdir()
    command = [str(binary), str(csv_path), str(angle_path), "--resources", "/src",
               "--config", str(config), "--tq-threads", "1"]
    result = subprocess.run(command, cwd="/src", capture_output=True, text=True, timeout=180)
    (work / "native.stdout").write_text(result.stdout)
    (work / "native.stderr").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"oracle exit {result.returncode}: {result.stderr[-1500:]}")
    metrics = dict(item.split("=", 1) for item in result.stdout.strip().split())
    assert float(metrics["min_energy_margin"]) >= 0
    assert float(metrics["min_heat_margin"]) >= 0
    with csv_path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 14 * 901, len(rows)
    cases = {}
    for row in rows:
        cases.setdefault(row["case"], []).append(row)
    assert len(cases) == 14
    for name, trace in cases.items():
        assert len(trace) == 901 and [int(r["tick"]) for r in trace] == list(range(901)), name
        initial, final = trace[0], trace[-1]
        assert (final["x"], final["y"]) != (initial["x"], initial["y"]), name
        assert float(initial["accel"]) > 0 and float(initial["drag"]) > 0, name
        if name.endswith("reverse_fixture"):
            assert float(initial["reverse_accel"]) > 0
    assert angle_path.stat().st_size == 65536 * 2 * 8
    resource_digest = hashlib.sha256()
    resource_count = 0
    for path in sorted(Path("/src/data").rglob("*")):
        if path.is_file():
            relative = path.relative_to("/src").as_posix().encode()
            resource_digest.update(len(relative).to_bytes(4, "little"))
            resource_digest.update(relative)
            resource_digest.update(hashlib.sha256(path.read_bytes()).digest())
            resource_count += 1
    record = {
        "schema": 1,
        "source_commit": Path("/baseline/source-commit.txt").read_text().strip(),
        "image_id": os.environ["MOTION_IMAGE_ID"],
        "baseline_binary_sha256": digest(build / "endless-sky"),
        "oracle_source_sha256": digest(source),
        "runner_source_sha256": digest(Path("/src/dos/motion/native.py")),
        "oracle_binary_sha256": digest(binary),
        "csv_sha256": digest(csv_path),
        "angles_sha256": digest(angle_path),
        "resource_data_sha256": resource_digest.hexdigest(),
        "resource_data_files": resource_count,
        "compiler_flags": flags,
        "rows": len(rows), "ticks_per_case": 900,
        "cases": sorted(cases),
        "initial_conditions": {
            "all": {"position": [125, -73], "heading_degrees": 0, "velocity": [0, 0],
                    "system": "Sol", "recharge": "All", "crew": "hire to required"},
            "coast_velocity": [2, -3], "back_velocity": [1, -1], "stop_velocity": [0.3, 0.01],
            "mixed_velocity": [1.25, -0.75], "mixed_heading_degrees": 359,
            "reverse_fixture": "Sparrow plus one X1100 Ion Reverse Thruster via public AddOutfit"},
        "schedule": {"forward": "forward all ticks", "coast": "no commands all ticks",
                     "turn": "forward, turn +1 all ticks", "back": "back all ticks",
                     "stop": "forward+STOP all ticks",
                     "pilot": "Same phase boundaries as mixed, full left/right turns; zero initial velocity and heading",
                     "mixed": "1-120 forward; 121-240 coast; 241-360 forward turn -.5; "
                              "361-480 back; 481-600 forward turn +.25; 601-720 forward+back; 721-900 coast"},
        "fields": "case,tick,cmd_forward,cmd_back,cmd_stop,cmd_turn,accel,reverse_accel,turn_rate,drag,mult,x,y,vx,vy,angle_steps",
        "resource_checks": "Every tick: crew >= required, not disabled/destroyed/NeedsEnergy, energy/fuel >= 0, hull > 0",
        "minimum_energy_margin": float(metrics["min_energy_margin"]),
        "minimum_heat_margin": float(metrics["min_heat_margin"]),
        "throttle_check": "Before each Move, AvailableResources.CanExpend(combined turn and thrust costs); selected fittings have only energy/heat movement costs",
        "stop_check": "First STOP tick leaves lateral velocity 0.3 and clamps facing-normal velocity to zero",
        "scope": "Standalone public Ship::Move, no Engine::Fire; bounded at 900 ticks before forget removal",
    }
    (work / "provenance.json").write_text(json.dumps(record, indent=2) + "\n")
    print(result.stdout.strip())


def outside():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline = (ROOT / ".work/native/source-commit.txt").read_text().strip()
    guarded = ["source", "CMakeLists.txt", "cmake"]
    for extra in ([baseline, "--"], ["--"], ["--cached", "--"]):
        subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", *extra, *guarded], check=True)
    unknown = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "--others",
                                       "--exclude-standard", "--", *guarded], text=True)
    if unknown:
        raise RuntimeError("Untracked upstream source/build input; rebuild native reference")
    image_id = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE],
                                       text=True).strip()
    subprocess.run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                    "--env", "HOME=/tmp", "--env", f"MOTION_IMAGE_ID={image_id}",
                    "--mount", f"type=bind,src={ROOT},dst=/src,readonly",
                    "--mount", f"type=bind,src={ROOT / '.work/native'},dst=/baseline,readonly",
                    "--mount", f"type=bind,src={WORK},dst=/work/motion", IMAGE,
                    "python3", "/src/dos/motion/native.py", "--inside"], check=True)


if __name__ == "__main__":
    try:
        inside() if sys.argv[1:] == ["--inside"] else outside()
    except (AssertionError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(f"motion oracle failed: {error}")
