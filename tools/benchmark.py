#!/usr/bin/env python3
"""Measure validated formatting and stdout work with reproducible resources."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import socket
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import conformance as qualification
from oracle import oracle_identity


def command(argv, directory, name, timeout=600):
    return qualification.run(argv, directory, name, timeout=timeout)


def fnv(data, repeats=1):
    value = 1469598103934665603
    for _ in range(repeats):
        for byte in data:
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return str(value)


def metrics(process):
    records = [line for line in process["stderr"].decode("utf-8", "replace").splitlines() if line.startswith('{"schema_version":1,')]
    if len(records) != 1:
        raise ValueError("Missing or duplicate benchmark JSON record")
    data = json.loads(records[0])
    samples = data["samples_ns"]
    if samples:
        if len(samples) != 30 or data["warmups"] != 5 or any(x < 0 for x in samples):
            raise ValueError("Invalid sample count or duration")
        data.update(median_ns=statistics.median(samples), min_ns=min(samples), max_ns=max(samples),
                    mean_ns=statistics.mean(samples), stdev_ns=statistics.stdev(samples))
        data["median_ns_per_call"] = data["median_ns"] / (data["iterations"] * data["calls"])
    if data.get("device_kernel_samples_ns"):
        if len(data["device_kernel_samples_ns"]) != 30:
            raise ValueError("Invalid device event sample count")
        data["median_device_kernel_ns"] = statistics.median(data["device_kernel_samples_ns"])
    data["device"] = [s.removeprefix("FFMT_BENCH_DEVICE ") if hasattr(s, "removeprefix") else s[len("FFMT_BENCH_DEVICE "):]
                      for s in process["stderr"].decode("utf-8", "replace").splitlines() if s.startswith("FFMT_BENCH_DEVICE ")]
    return data


def link_flags(flags):
    # Source-language forcing applies to compilation, not to already-built .o.
    result, skip = [], False
    for flag in flags:
        if skip:
            skip = False
        elif flag == "-x":
            skip = True
        else:
            result.append(flag)
    return result



def build(route, capacity, multi, destination, include, reference=False, sources_directory=None, float_presentation="explicit_e8"):
    cxx = qualification.compiler(route)
    flags = qualification.flags(route, 2)
    defines = [f"-DFFMT_BUFFER_SIZE={capacity}"]
    if float_presentation == "shortest":
        defines += ["-DBENCH_SHORTEST=1"]
    if reference:
        cxx = qualification.compiler(route) if route.startswith("host-") else shlex.split(os.environ.get("FFMT_HOST_CXX", "g++"))
        flags = ["-std=c++20", "-O2", "-fno-fast-math"]
        defines += ["-DBENCH_REFERENCE"]
    elif route in ("cuda", "nvcc"):
        defines += ["-DBENCH_CUDA"]
    elif route in ("hip", "chipstar"):
        defines += ["-DBENCH_HIP"]
    elif route in ("sycl", "acpp"):
        defines += ["-DBENCH_SYCL"]
    elif route.startswith("omp-"):
        defines += ["-DBENCH_OMP"]
    if multi:
        defines += ["-DBENCH_MULTI_TU"]
        if not reference:
            if route == "nvcc": flags += ["-rdc=true"]
            elif route in ("cuda", "hip"): flags += ["-fgpu-rdc"]
    destination.mkdir(parents=True, exist_ok=True)
    sources_directory = sources_directory or ROOT / "test/conformance"
    sources = [sources_directory / "benchmark.cpp"]
    if multi:
        sources += [sources_directory / "benchmark_extra.cpp"]
    events, objects = [], []
    for index, source in enumerate(sources):
        obj = destination / f"unit-{index}.o"
        cmd = [*cxx, *flags, *defines, "-I" + str(include),
               "-I" + str(sources_directory), "-c", str(source), "-o", str(obj)]
        process = command(cmd, destination, f"compile-{index}")
        events.append(qualification.record(process))
        if process["exit_code"]:
            return {"status": "build_failure", "events": events}
        objects.append(obj)
    if reference:
        obj = destination / "fmt.o"
        process = command([*cxx, "-std=c++20", "-O2", "-fno-fast-math", "-I" + str(include), "-c",
                           str(include.parent / "src/format.cc"), "-o", str(obj)], destination, "compile-fmt")
        events.append(qualification.record(process))
        if process["exit_code"]:
            return {"status": "oracle_failure", "events": events}
        objects.append(obj)
    executable = destination / "benchmark"
    process = command([*cxx, *link_flags(flags), *map(str, objects), "-o", str(executable)], destination, "link")
    events.append(qualification.record(process))
    if process["exit_code"]:
        return {"status": "link_failure", "events": events}
    return {"status": "built", "executable": str(executable), "events": events,
            "clean_compile_seconds": sum(event["seconds"] for event in events),
            "executable_bytes": executable.stat().st_size,
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "object_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in objects}}




def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profiles", default=os.environ.get("FFMT_PROFILES_FILE"), help="Optional JSON profile defaults")
    p.add_argument("--profile", default="local", help="Profile label/default routes; built-in profile: local")
    p.add_argument("--source-root", default=str(qualification.SOURCE_ROOT), help="Library Git checkout to snapshot independently of the benchmark runner")
    p.add_argument("--fmt-dir", type=Path, default=Path(os.environ.get("FFMT_FMT_DIR", qualification.WORKSPACE / "third_party/fmt")), help="Checkout of the portable suite's pinned host fmt oracle")
    p.add_argument("--revision", default="WORKTREE")
    p.add_argument("--route", action="append", choices=[route for route in qualification.ROUTES if not route.startswith("kokkos-")])
    p.add_argument("--capacity", type=int, action="append", choices=[16, 64, 128, 512])
    p.add_argument("--family", action="append", choices=["integer", "float"], help="Restrict timing workloads to selected value families")
    p.add_argument("--mode", action="append", choices=["memory", "print"], help="Restrict timing workloads to selected output modes")
    p.add_argument("--float-presentation", choices=["explicit_e8", "shortest"], default="explicit_e8", help="Shortest timing requires memory mode and compares only with fmt")
    p.add_argument("--memory-iterations", type=int, default=4096)
    p.add_argument("--print-iterations", type=int, default=128)
    p.add_argument("--multi-tu", action="store_true", help="Also measure separate translation units")
    p.add_argument("--output")
    a = p.parse_args()
    try:
        profiles = qualification.load_profiles(a.profiles)
    except (OSError, ValueError) as error:
        p.error(str(error))
    if a.profile not in profiles["profiles"]:
        p.error("unknown profile %s; supply --profiles or use local" % a.profile)
    qualification.SOURCE_ROOT = Path(a.source_root).resolve()
    a.fmt_dir = a.fmt_dir.resolve()
    os.environ["FFMT_PROFILE"] = a.profile
    if a.float_presentation == "shortest" and (set(a.mode or ["memory", "print"]) != {"memory"}):
        p.error("--float-presentation shortest requires --mode memory")
    # Keep JIT preparation reproducible across independent verification and
    # timing processes. Intel's executable kernel bundle is built explicitly.
    os.environ["SYCL_CACHE_PERSISTENT"] = "0"
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    output = (qualification.WORKSPACE / a.output if a.output else qualification.WORKSPACE / "artifacts" / a.profile / ("benchmark-" + stamp)).resolve()
    output.mkdir(parents=True, exist_ok=False)
    hashes = qualification.snapshot(output / "source", a.revision)
    bench_sources = output / "source/benchmark"
    bench_sources.mkdir(parents=True, exist_ok=True)
    for path in (ROOT / "test/conformance").glob("benchmark*"):
        if path.is_file(): (bench_sources / path.name).write_bytes(path.read_bytes())
    if not (a.fmt_dir / "src/format.cc").exists():
        p.error("Provide the pinned fmt oracle checkout with --fmt-dir")
    try:
        oracle = oracle_identity(a.fmt_dir)
    except ValueError as error:
        p.error(str(error))
    oracle_commit = oracle["commit"]
    manifest = {"schema_version": 1, "profile": a.profile, "revision": a.revision, "source_files": hashes,
                "started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "source_checkout": str(qualification.SOURCE_ROOT),
                "source_commit": qualification.source_git("rev-parse", "HEAD" if a.revision == "WORKTREE" else a.revision),
                "source_worktree": a.revision == "WORKTREE", "runner_commit": qualification.git("rev-parse", "HEAD"),
                "source_dirty": bool(qualification.source_git("status", "--porcelain")),
                "runner_checkout": str(ROOT), "workspace": str(qualification.WORKSPACE),
                "invocation": [sys.executable, *sys.argv],
                "hostname": socket.gethostname(), "machine": platform.machine(),
                "fmt_commit": oracle_commit, "oracle": oracle, "timer_policy": "host wall; accelerator launch and synchronization included; copies and checksum outside timer; first_batch is first invocation in this process, not an implicit cold-JIT claim",
                "sycl_cache_persistent": False, "sycl_jit_policy": "explicit executable kernel bundle build before batches (Intel SYCL); preparation includes backend cache lookup and compilation",
                "warmups": 5, "samples": 30, "results": [], "builds": []}
    manifest["runner_files"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted((ROOT / "tools").glob("*.py"))}
    manifest["environment"] = {key: os.environ[key] for key in
        ("SYCL_CACHE_PERSISTENT", "ONEAPI_DEVICE_SELECTOR", "CHIP_BE", "CHIP_DEVICE_TYPE", "CHIP_SKIP_UNINIT") if key in os.environ}
    manifest["selection"] = dict(families=a.family or ["integer", "float"], modes=a.mode or ["memory", "print"],
                                 capacities=a.capacity or [16, 64, 128, 512], multi_tu=a.multi_tu,
                                 float_presentation=a.float_presentation)
    if a.profiles:
        profile_path = Path(a.profiles).resolve()
        manifest["profiles"] = dict(path=str(profile_path), sha256=hashlib.sha256(profile_path.read_bytes()).hexdigest())
    manifest["compiler_versions"] = {}
    manifest["benchmark_sources"] = {str(Path("test/conformance") / path.name): hashlib.sha256(path.read_bytes()).hexdigest()
                                     for path in bench_sources.iterdir() if path.is_file()}

    def save():
        temporary = output / "manifest.json.tmp"
        temporary.write_text(json.dumps(manifest, indent=2) + "\n")
        temporary.replace(output / "manifest.json")

    failed = False
    for route in a.route or profiles["profiles"][a.profile]["routes"]:
        version = command([*qualification.compiler(route), "--version"], output / route, "compiler-version")
        manifest["compiler_versions"][route] = dict(**qualification.record(version), text=version["stdout"].decode("utf-8", "replace"))
        oracle = build(route, 128, False, output / route / "oracle", a.fmt_dir / "include", True, bench_sources, a.float_presentation)
        manifest["builds"].append(dict(route=route, engine="fmt", **oracle)); save()
        if oracle["status"] != "built":
            failed = True
            continue
        for capacity in a.capacity or [16, 64, 128, 512]:
            for multi in ([False, True] if a.multi_tu else [False]):
                config = output / route / f"capacity-{capacity}" / ("multi-tu" if multi else "single-tu")
                candidate = build(route, capacity, multi, config, output / "source/include", sources_directory=bench_sources, float_presentation=a.float_presentation)
                manifest["builds"].append(dict(route=route, capacity=capacity, multi_tu=multi, engine="ffmt", **candidate)); save()
                if candidate["status"] != "built":
                    failed = True
                    continue
                for floating in (0, 1):
                    if a.family and ("float" if floating else "integer") not in a.family:
                        continue
                    for printing in (0, 1):
                        if a.mode and ("print" if printing else "memory") not in a.mode:
                            continue
                        for calls in (1, 2):
                            count = a.print_iterations if printing else a.memory_iterations
                            mode = f"{'float' if floating else 'integer'}-{'print' if printing else 'memory'}-{calls}calls"
                            engines = [("ffmt", candidate["executable"], 0)]
                            if printing:
                                engines += [("printf", candidate["executable"], 1)]
                            elif route.startswith("host-"):
                                engines += [("fmt", oracle["executable"], 0)]
                            expected = command([oracle["executable"], floating, printing, 0, calls, count, 1], config, mode + "-oracle")
                            for engine, executable, native in engines:
                                row = dict(route=route, capacity=capacity, multi_tu=multi, engine=engine,
                                           family="float" if floating else "integer", presentation=a.float_presentation if floating else "integer_d",
                                           mode="print" if printing else "memory", calls=calls)
                                check = command([executable, floating, printing, native, calls, count, 1], config, mode + "-" + engine + "-verify")
                                row["verification_process_seconds"] = check["seconds"]
                                if expected["exit_code"] or check["exit_code"]:
                                    row.update(status="execution_failure", verification=qualification.record(check))
                                elif expected["stdout"] != check["stdout"]:
                                    row.update(status="mismatch", verification=qualification.compare_bytes(expected["stdout"], check["stdout"]))
                                else:
                                    process = command([executable, floating, printing, native, calls, count, 0], config, mode + "-" + engine + "-measure")
                                    row["execution"] = qualification.record(process)
                                    try:
                                        data = metrics(process)
                                        valid = (process["exit_code"] == 0 and len(data["samples_ns"]) == 30)
                                        valid &= (process["stdout"] == expected["stdout"] * 36) if printing else (data["checksum"] == fnv(expected["stdout"], 36))
                                        row.update(status="measured" if valid else "measurement_invalid", metrics=data)
                                    except (ValueError, KeyError, json.JSONDecodeError) as exc:
                                        row.update(status="measurement_invalid", error=str(exc))
                                failed |= row["status"] != "measured"
                                manifest["results"].append(row); save()
                                print(route, capacity, mode, engine, row["status"], flush=True)
    manifest["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
    print("Manifest:", output / "manifest.json")
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
