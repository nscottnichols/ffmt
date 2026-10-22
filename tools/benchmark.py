#!/usr/bin/env python3
"""Measure validated formatting and stdout work with reproducible resources."""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import socket
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import conformance as qualification


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


def build(route, capacity, multi, destination, include, reference=False, sources_directory=None):
    cxx = qualification.compiler(route)
    flags = qualification.flags(route, 2)
    defines = [f"-DFFMT_BUFFER_SIZE={capacity}"]
    if reference:
        cxx = qualification.compiler(route) if route.startswith("host-") else [os.environ.get("FFMT_HOST_CXX", "g++")]
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
    resource = []
    if route == "nvcc" and not reference:
        resource = ["--resource-usage", "-Xptxas=-v"]
    elif route in ("host-buffer", "host-specifiers") or reference:
        resource = ["-fstack-usage"]
    destination.mkdir(parents=True, exist_ok=True)
    sources_directory = sources_directory or ROOT / "test/conformance"
    sources = [sources_directory / "benchmark.cpp"]
    if multi:
        sources += [sources_directory / "benchmark_extra.cpp"]
    events, objects = [], []
    for index, source in enumerate(sources):
        obj = destination / f"unit-{index}.o"
        cmd = [*cxx, *flags, *defines, *resource, "-I" + str(include),
               "-I" + str(sources_directory), "-c", str(source), "-o", str(obj)]
        process = command(cmd, destination, f"compile-{index}")
        events.append(qualification.record(process))
        if process["exit_code"]:
            return {"status": "build_failure", "events": events}
        objects.append(obj)
    if reference:
        obj = destination / "fmt.o"
        process = command([*cxx, "-std=c++20", "-O2", "-fno-fast-math", "-I" + str(include), "-c",
                           str(ROOT / "third_party/fmt/src/format.cc"), "-o", str(obj)], destination, "compile-fmt")
        events.append(qualification.record(process))
        if process["exit_code"]:
            return {"status": "oracle_failure", "events": events}
        objects.append(obj)
    executable = destination / "benchmark"
    process = command([*cxx, *link_flags(flags), *map(str, objects), "-o", str(executable)], destination, "link")
    events.append(qualification.record(process))
    if process["exit_code"]:
        return {"status": "link_failure", "events": events}
    sizes = []
    if shutil.which("size"):
        process = command(["size", "-A", *map(str, objects), str(executable)], destination, "size")
        sizes.append(qualification.record(process))
    image_metadata = []
    inspect = ["cuobjdump", "--list-elf", str(executable)] if route == "nvcc" and not reference else ["llvm-objdump", "--offloading", str(objects[0])]
    if not reference and not route.startswith("host-") and shutil.which(inspect[0]):
        process = command(inspect, destination, "device-images")
        image_metadata.append(dict(**qualification.record(process), text=process["stdout"].decode("utf-8", "replace")))
    return {"status": "built", "executable": str(executable), "events": events,
            "clean_compile_seconds": sum(e["seconds"] for e in events), "resources": sizes,
            "object_bytes": {p.name: p.stat().st_size for p in objects},
            "executable_bytes": executable.stat().st_size,
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "object_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in objects},
            "device_image_metadata": image_metadata}


def resources(route, capacity, destination, include, sources_directory):
    rows = []
    macro = "CUDA" if route in ("cuda", "nvcc") else "HIP" if route in ("hip", "chipstar") else "SYCL" if route in ("sycl", "acpp") else "OMP" if route.startswith("omp-") else None
    extra = ["--resource-usage", "-Xptxas=-v"] if route == "nvcc" else ["-fstack-usage"]
    if route == "hip": extra += ["-Rpass-analysis=kernel-resource-usage"]
    for floating in (0, 1):
        for printing in (0, 1):
            for calls in (1, 2):
                label = f"{'float' if floating else 'integer'}-{'print' if printing else 'memory'}-{calls}calls"
                directory = destination / label
                directory.mkdir(parents=True, exist_ok=True)
                obj = directory / "resource.o"
                defines = [f"-DFFMT_BUFFER_SIZE={capacity}", f"-DBENCH_FLOAT={floating}", f"-DBENCH_PRINT={printing}", f"-DBENCH_CALLS={calls}"]
                if macro: defines += ["-DBENCH_" + macro]
                process = command([*qualification.compiler(route), *qualification.flags(route, 2), *extra, *defines,
                                   "-I"+str(include), "-I"+str(sources_directory), "-c", str(sources_directory/"benchmark_resource.cpp"), "-o", str(obj)], directory, "compile")
                row = dict(route=route,capacity=capacity,family="float" if floating else "integer",mode="print" if printing else "memory",calls=calls,
                           status="compiled" if process["exit_code"] == 0 else "compile_failure",compile=qualification.record(process))
                if obj.exists(): row["object_bytes"] = obj.stat().st_size
                diagnostic = process["stderr"].decode("utf-8", "replace")
                for key, pattern in [("gpu_registers", r"Used (\d+) registers"), ("gpu_stack_frame_bytes", r"(\d+) bytes stack frame"),
                                     ("gpu_spill_store_bytes", r"(\d+) bytes spill stores"), ("gpu_spill_load_bytes", r"(\d+) bytes spill loads"),
                                     ("gpu_vgprs", r"(?:VGPRs|NumVgprs):\s*(\d+)"), ("gpu_sgprs", r"(?:SGPRs|NumSgprs):\s*(\d+)")]:
                    values = [int(v) for v in re.findall(pattern, diagnostic)]
                    if values: row[key] = values
                stack = []
                for report in directory.glob("*.su"):
                    for line in report.read_text().splitlines():
                        fields = line.rsplit("\t", 2)
                        if len(fields) == 3 and fields[1].isdigit(): stack.append(int(fields[1]))
                if stack:
                    row["max_static_frame_bytes"] = max(stack)
                    row["stack_frame_scope"] = "maximum compiler .su frame; excludes cumulative call depth"
                if obj.exists() and shutil.which("size"):
                    section = command(["size", "-A", str(obj)], directory, "size")
                    row["section_report"] = qualification.record(section)
                    sections = []
                    for line in section["stdout"].decode("utf-8", "replace").splitlines():
                        fields = line.split()
                        if len(fields) >= 2 and fields[0].startswith(".") and fields[1].isdigit(): sections.append((fields[0], int(fields[1])))
                    row["text_section_bytes"] = sum(size for name, size in sections if name.startswith(".text"))
                    row["read_only_section_bytes"] = sum(size for name, size in sections if name.startswith(".rodata"))
                rows.append(row)
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", default="local", choices=qualification.CONFIG["profiles"])
    p.add_argument("--revision", default="WORKTREE")
    p.add_argument("--route", action="append", choices=["host-buffer", "host-specifiers", "cuda", "nvcc", "hip",
                   "omp-cuda", "omp-hip", "omp-spirv", "sycl", "acpp", "chipstar"])
    p.add_argument("--capacity", type=int, action="append", choices=[16, 64, 128, 512])
    p.add_argument("--memory-iterations", type=int, default=4096)
    p.add_argument("--print-iterations", type=int, default=128)
    p.add_argument("--multi-tu", action="store_true", help="Also measure separate translation units")
    p.add_argument("--resources-only", action="store_true", help="Compile dedicated integer/float one/two-call resource probes without timing")
    p.add_argument("--output")
    a = p.parse_args()
    # Keep JIT preparation reproducible across independent verification and
    # timing processes. Intel's executable kernel bundle is built explicitly.
    os.environ["SYCL_CACHE_PERSISTENT"] = "0"
    if a.profile != "local" and not os.environ.get("COBALT_JOBID"):
        p.error("Representative measurements require an active Cobalt allocation")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = ROOT / a.output if a.output else ROOT / "artifacts" / a.profile / ("benchmark-" + stamp)
    output.mkdir(parents=True, exist_ok=True)
    hashes = qualification.snapshot(output / "source", a.revision)
    bench_sources = output / "source/benchmark"
    bench_sources.mkdir(parents=True, exist_ok=True)
    for path in (ROOT / "test/conformance").glob("benchmark*"):
        if path.is_file(): (bench_sources / path.name).write_bytes(path.read_bytes())
    if not (ROOT / "third_party/fmt/src/format.cc").exists():
        p.error("Provision the pinned fmt oracle with tools/conformance.py first")
    oracle_commit = subprocess.check_output(["git", "-C", str(ROOT / "third_party/fmt"), "rev-parse", "HEAD"]).decode().strip()
    if oracle_commit != qualification.CONFIG["fmt_commit"]:
        p.error("fmt oracle commit differs from the campaign pin")
    manifest = {"schema_version": 1, "profile": a.profile, "revision": a.revision, "source_files": hashes,
                "started": datetime.datetime.now(datetime.timezone.utc).isoformat(), "source_commit": qualification.git("rev-parse", "HEAD"),
                "hostname": socket.gethostname(), "machine": platform.machine(), "cobalt_jobid": os.environ.get("COBALT_JOBID"),
                "fmt_commit": oracle_commit, "timer_policy": "host wall; accelerator launch and synchronization included; copies and checksum outside timer; first_batch is first invocation in this process, not an implicit cold-JIT claim",
                "sycl_cache_persistent": False, "sycl_jit_policy": "explicit executable kernel bundle build before batches (Intel SYCL); preparation includes backend cache lookup and compilation",
                "warmups": 5, "samples": 30, "results": [], "builds": [], "resource_probes": []}
    discovery = ROOT / "artifacts" / a.profile / ("discovery-"+os.environ.get("COBALT_JOBID", "manual")+".json")
    if discovery.exists():
        manifest["discovery"] = dict(path=str(discovery.relative_to(ROOT)), sha256=hashlib.sha256(discovery.read_bytes()).hexdigest())
    manifest["compiler_versions"] = {}
    manifest["benchmark_sources"] = {str(Path("test/conformance") / path.name): hashlib.sha256(path.read_bytes()).hexdigest()
                                     for path in bench_sources.iterdir() if path.is_file()}

    def save():
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    failed = False
    for route in a.route or qualification.CONFIG["profiles"][a.profile]["routes"]:
        version = command([*qualification.compiler(route), "--version"], output / route, "compiler-version")
        manifest["compiler_versions"][route] = dict(**qualification.record(version), text=version["stdout"].decode("utf-8", "replace"))
        if a.resources_only:
            for capacity in a.capacity or [16, 64, 128, 512]:
                probes = resources(route, capacity, output / route / f"capacity-{capacity}" / "resources", output / "source/include", bench_sources)
                manifest["resource_probes"] += probes
                failed |= any(row["status"] != "compiled" for row in probes)
                save()
            continue
        oracle = build(route, 128, False, output / route / "oracle", ROOT / "third_party/fmt/include", True, bench_sources)
        manifest["builds"].append(dict(route=route, engine="fmt", **oracle)); save()
        if oracle["status"] != "built":
            failed = True
            continue
        for capacity in a.capacity or [16, 64, 128, 512]:
            for multi in ([False, True] if a.multi_tu else [False]):
                config = output / route / f"capacity-{capacity}" / ("multi-tu" if multi else "single-tu")
                candidate = build(route, capacity, multi, config, output / "source/include", sources_directory=bench_sources)
                manifest["builds"].append(dict(route=route, capacity=capacity, multi_tu=multi, engine="ffmt", **candidate)); save()
                if candidate["status"] != "built":
                    failed = True
                    continue
                for floating in (0, 1):
                    for printing in (0, 1):
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
                                           family="float" if floating else "integer", mode="print" if printing else "memory", calls=calls)
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
