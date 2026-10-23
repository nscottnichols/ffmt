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


def image_tables(route, obj, directory):
    """Keep ELF images and count retained conversion tables by symbol and size."""
    images, events = [], []
    if route == "nvcc" and shutil.which("cuobjdump"):
        inspect = command(["cuobjdump", "--dump-elf-symbols", "--dump-resource-usage", str(obj)], directory, "cuda-symbols")
        events.append(qualification.record(inspect))
        # cuobjdump writes extracted files in its cwd. Keep them with this
        # isolated object rather than in the repository working directory.
        process = command([sys.executable, "-c", "import os,sys;os.chdir(sys.argv[1]);os.execvp(sys.argv[2],sys.argv[2:])",
                           str(directory), "cuobjdump", "--extract-elf", "all", str(obj)], directory, "cuda-extract")
        events.append(qualification.record(process))
        images = sorted(directory.glob("*.cubin"))
    elif route == "hip" and shutil.which("llvm-objdump"):
        process = command(["llvm-objdump", "--offloading", str(obj)], directory, "hip-extract")
        events.append(qualification.record(process))
        images = sorted(directory.glob(obj.name+".*amdgcn*"))
    elif route.startswith("host-"):
        images = [obj]
    result = dict(events=events, images=[], tables=[])
    needles = ("float_pow10_table", "double_pow10_significands", "double_powers_of_5_64", "wide_cp_ranges")
    for index, path in enumerate(images):
        result["images"].append(dict(path=qualification.path_label(path), bytes=path.stat().st_size,
                                      sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
        if not shutil.which("readelf"):
            continue
        symbols = command(["readelf", "--symbols", "--wide", str(path)], directory, "image-"+str(index)+"-symbols")
        result["events"].append(qualification.record(symbols))
        for line in symbols["stdout"].decode("utf-8", "replace").splitlines():
            fields = line.split()
            if len(fields) < 8 or fields[3] != "OBJECT":
                continue
            table = next((name for name in needles if name in fields[7]), None)
            if table:
                result["tables"].append(dict(table=table, symbol=fields[7], bytes=int(fields[2], 0) if fields[2].startswith("0x") else int(fields[2]),
                                              image=qualification.path_label(path)))
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
    sizes = []
    if shutil.which("size"):
        process = command(["size", "-A", *map(str, objects), str(executable)], destination, "size")
        sizes.append(qualification.record(process))
    image_metadata = []
    inspect = ["cuobjdump", "--list-elf", str(executable)] if route == "nvcc" and not reference else ["llvm-objdump", "--offloading", str(objects[0])]
    if not reference and not route.startswith("host-") and shutil.which(inspect[0]):
        process = command(inspect, destination, "device-images")
        image_metadata.append(dict(**qualification.record(process), text=process["stdout"].decode("utf-8", "replace")))
    tables = image_tables(route, executable, destination) if not reference else {}
    return {"status": "built", "executable": str(executable), "events": events,
            "clean_compile_seconds": sum(e["seconds"] for e in events), "resources": sizes,
            "object_bytes": {p.name: p.stat().st_size for p in objects},
            "executable_bytes": executable.stat().st_size,
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "object_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in objects},
            "device_image_metadata": image_metadata, "conversion_tables": tables}


def resources(route, capacity, destination, include, sources_directory, presentation="existing", families=None, modes=None):
    rows = []
    macro = "CUDA" if route in ("cuda", "nvcc") else "HIP" if route in ("hip", "chipstar") else "SYCL" if route in ("sycl", "acpp") else "OMP" if route.startswith("omp-") else None
    extra = ["--resource-usage", "-Xptxas=-v"] if route == "nvcc" else ["-fstack-usage"]
    if route == "hip": extra += ["-Rpass-analysis=kernel-resource-usage"]
    for floating in ((0, 1) if presentation == "existing" else (2,) if presentation == "shortest" else (0, 1, 2)):
        if families and ("float" if floating else "integer") not in families:
            continue
        for printing in (0, 1):
            if modes and ("print" if printing else "memory") not in modes:
                continue
            for calls in (1, 2):
                label = f"{'float-shortest' if floating == 2 else 'float' if floating else 'integer'}-{'print' if printing else 'memory'}-{calls}calls"
                directory = destination / label
                directory.mkdir(parents=True, exist_ok=True)
                obj = directory / "resource.o"
                defines = [f"-DFFMT_BUFFER_SIZE={capacity}", f"-DBENCH_FLOAT={int(bool(floating))}", f"-DBENCH_PRINT={printing}", f"-DBENCH_CALLS={calls}", f"-DBENCH_SHORTEST={int(floating == 2)}"]
                if macro: defines += ["-DBENCH_" + macro]
                process = command([*qualification.compiler(route), *qualification.flags(route, 2), *extra, *defines,
                                   "-I"+str(include), "-I"+str(sources_directory), "-c", str(sources_directory/"benchmark_resource.cpp"), "-o", str(obj)], directory, "compile")
                row = dict(route=route,capacity=capacity,family="float" if floating else "integer",presentation="shortest" if floating == 2 else "explicit_e8" if floating else "integer_d",mode="print" if printing else "memory",calls=calls,
                           status="compiled" if process["exit_code"] == 0 else "compile_failure",compile=qualification.record(process))
                if obj.exists(): row["object_bytes"] = obj.stat().st_size
                diagnostic = process["stderr"].decode("utf-8", "replace")
                for key, pattern in [("gpu_registers", r"Used (\d+) registers"), ("gpu_stack_frame_bytes", r"(\d+) bytes stack frame"),
                                     ("gpu_spill_store_bytes", r"(\d+) bytes spill stores"), ("gpu_spill_load_bytes", r"(\d+) bytes spill loads"),
                                     ("gpu_vgprs", r"(?:VGPRs|NumVgprs):\s*(\d+)"), ("gpu_sgprs", r"(?:SGPRs|NumSgprs):\s*(\d+)"),
                                     ("gpu_scratch_bytes_per_lane", r"ScratchSize \[bytes/lane\]:\s*(\d+)")]:
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
                if obj.exists():
                    row["conversion_tables"] = image_tables(route, obj, directory)
                rows.append(row)
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profiles", default=os.environ.get("FFMT_PROFILES_FILE"), help="Optional JSON profile defaults")
    p.add_argument("--profile", default="local", help="Profile label/default routes; built-in profile: local")
    p.add_argument("--source-root", default=str(qualification.SOURCE_ROOT), help="Library Git checkout to snapshot independently of the benchmark runner")
    p.add_argument("--fmt-dir", type=Path, default=Path(os.environ.get("FFMT_FMT_DIR", qualification.WORKSPACE / "third_party/fmt")), help="Checkout of the portable suite's pinned host fmt oracle")
    p.add_argument("--revision", default="WORKTREE")
    p.add_argument("--route", action="append", choices=[route for route in qualification.ROUTES if not route.startswith("kokkos-")])
    p.add_argument("--capacity", type=int, action="append", choices=[16, 64, 128, 512])
    p.add_argument("--family", action="append", choices=["integer", "float"], help="Restrict timing/resource probes to selected value families")
    p.add_argument("--mode", action="append", choices=["memory", "print"], help="Restrict timing/resource probes to selected output modes")
    p.add_argument("--float-presentation", choices=["explicit_e8", "shortest"], default="explicit_e8", help="Shortest timing requires memory mode and compares only with fmt")
    p.add_argument("--memory-iterations", type=int, default=4096)
    p.add_argument("--print-iterations", type=int, default=128)
    p.add_argument("--multi-tu", action="store_true", help="Also measure separate translation units")
    p.add_argument("--resources-only", action="store_true", help="Compile dedicated integer/float one/two-call resource probes without timing")
    p.add_argument("--resource-presentation", choices=["existing", "shortest", "both"], default="existing", help="Resource-only float presentation; existing rows use explicit8e precision")
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
    if a.float_presentation == "shortest" and (a.resources_only or set(a.mode or ["memory", "print"]) != {"memory"}):
        p.error("--float-presentation shortest requires --mode memory; use --resource-presentation for resource probes")
    if a.resources_only and a.resource_presentation == "shortest" and a.family and "float" not in a.family:
        p.error("shortest resource probes require the float family")
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
                "warmups": 5, "samples": 30, "results": [], "builds": [], "resource_probes": []}
    manifest["runner_files"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted((ROOT / "tools").glob("*.py"))}
    manifest["environment"] = {key: os.environ[key] for key in
        ("SYCL_CACHE_PERSISTENT", "ONEAPI_DEVICE_SELECTOR", "CHIP_BE", "CHIP_DEVICE_TYPE", "CHIP_SKIP_UNINIT") if key in os.environ}
    manifest["selection"] = dict(families=a.family or ["integer", "float"], modes=a.mode or ["memory", "print"],
                                 capacities=a.capacity or [16, 64, 128, 512], multi_tu=a.multi_tu,
                                 resource_presentation=a.resource_presentation, float_presentation=a.float_presentation)
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
        if a.resources_only:
            for capacity in a.capacity or [16, 64, 128, 512]:
                probes = resources(route, capacity, output / route / f"capacity-{capacity}" / "resources", output / "source/include", bench_sources, a.resource_presentation, a.family, a.mode)
                manifest["resource_probes"] += probes
                failed |= any(row["status"] != "compiled" for row in probes)
                save()
            continue
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
