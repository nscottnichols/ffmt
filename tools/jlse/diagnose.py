#!/usr/bin/env python3
"""Inspect device permissions and Intel loader variants within an allocation."""
import argparse
import datetime
import glob
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time
import hashlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import conformance

ROOT = Path(__file__).resolve().parents[2]
ZE_PROBE = r"""
import ctypes,json
rows=[]
for name in ('libze_loader.so.1','libze_loader.so'):
 try:
  lib=ctypes.CDLL(name)
  init=lib.zeInit(1)
  count=ctypes.c_uint32()
  drivers=lib.zeDriverGet(ctypes.byref(count),None)
  rows.append(dict(loader=name,init=init,driver_result=drivers,driver_count=count.value))
 except Exception as e: rows.append(dict(loader=name,error=str(e)))
print(json.dumps(rows))
"""


def run(argv, env, timeout=60, stream=None):
    start = time.monotonic()
    try:
        if stream is None:
            result = subprocess.run(argv, capture_output=True, timeout=timeout, env=env)
            stdout, stderr = result.stdout, result.stderr
        else:
            with Path(str(stream)+".stdout").open("wb") as out, Path(str(stream)+".stderr").open("wb") as err:
                result = subprocess.run(argv, stdout=out, stderr=err, timeout=timeout, env=env)
            stdout, stderr = Path(str(stream)+".stdout").read_bytes(), Path(str(stream)+".stderr").read_bytes()
        return dict(command=argv, returncode=result.returncode, seconds=time.monotonic()-start,
                    stdout=stdout.decode("utf-8", "replace"), stderr=stderr.decode("utf-8", "replace"))
    except subprocess.TimeoutExpired as exc:
        stdout, stderr = exc.output or b"", exc.stderr or b""
        if stream is not None:
            stdout, stderr = Path(str(stream)+".stdout").read_bytes(), Path(str(stream)+".stderr").read_bytes()
        return dict(command=argv,timeout=True,seconds=time.monotonic()-start,error=str(exc),stdout=stdout.decode("utf-8","replace"),stderr=stderr.decode("utf-8","replace"))
    except (OSError, subprocess.SubprocessError) as exc:
        return dict(command=argv, error=str(exc))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", required=True)
    p.add_argument("--executable", help="Existing small offload control to run under every loader variant")
    p.add_argument("--omp-probes", action="store_true", help="Minimize direct printf, memory formatting, and literal stdout transport")
    p.add_argument("--numeric-probes", action="store_true", help="Isolate Dragonbox conversion and SYCL optimization stages")
    p.add_argument("--omp-stack-probes", action="store_true", help="Retry an immutable numeric executable with OpenMP stack and teams settings")
    p.add_argument("--omp-memory-probes", action="store_true", help="Minimize mapping, integer/float conversion, and teams constructs independently")
    p.add_argument("--omp-large-probes", action="store_true", help="Run each isolated mapping/conversion probe with the full8216-record allocation")
    p.add_argument("--expected", type=Path, help="Independent numeric oracle bytes for --omp-stack-probes")
    p.add_argument("--configured-only", action="store_true", help="Run the selected environment without the loader comparison matrix")
    p.add_argument("--timeout", type=int, default=60, help="Timeout for an existing offload executable")
    p.add_argument("--trace-sycl", action="store_true", help="Record Unified Runtime calls from the existing executable")
    a = p.parse_args()
    if a.omp_stack_probes and (not a.executable or not a.expected):
        p.error("--omp-stack-probes requires --executable and --expected")
    if not os.environ.get("COBALT_JOBID"):
        p.error("An active Cobalt allocation is required")
    base = dict(os.environ)
    parts = base.get("LD_LIBRARY_PATH", "").split(os.pathsep)
    native = dict(base)
    native["LD_LIBRARY_PATH"] = os.pathsep.join(p for p in parts if p and "intel-gpu-umd" not in p and "khronos" not in p)
    driver1032 = dict(native)
    driver1032["LD_LIBRARY_PATH"] = "/soft/libraries/intel-gpu-umd/1032.21/lib64:/soft/libraries/intel-gpu-umd/1032.21/lib64/intel-opencl:" + native["LD_LIBRARY_PATH"]
    evidence = dict(schema_version=1, profile=a.profile, hostname=socket.gethostname(),
                    cobalt_jobid=os.environ.get("COBALT_JOBID"),
                    timestamp_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    device_files={}, variants={})
    if a.omp_memory_probes or a.omp_large_probes:
        import omp_memory_probe
        evidence["omp_memory_probes"] = omp_memory_probe.run_probes(a.profile, a.timeout, a.omp_large_probes)
    if a.omp_stack_probes:
        import numeric_memory
        probe_dir = ROOT / "artifacts" / a.profile / f"omp-stack-{os.environ['COBALT_JOBID']}"
        probe_dir.mkdir(parents=True, exist_ok=True)
        expected = a.expected.read_bytes()
        evidence["omp_stack_probes"] = []
        evidence["executable_sha256"] = hashlib.sha256(Path(a.executable).read_bytes()).hexdigest()
        evidence["oracle_sha256"] = hashlib.sha256(expected).hexdigest()
        for stack, threads in [(65536, 64), (1048576, 64), (1048576, 1)]:
            label = f"stack-{stack}-threads-{threads}"
            extra = {"LIBOMPTARGET_STACK_SIZE": str(stack), "OMP_TEAMS_THREAD_LIMIT": str(threads)}
            prefix = probe_dir / label
            execution = run([a.executable], dict(base, **extra), a.timeout, prefix)
            execution.pop("stdout", None)
            row = dict(environment=extra, execution=execution, stdout_file=str(prefix)+".stdout", stderr_file=str(prefix)+".stderr")
            row["status"] = "execution_failure"
            if execution.get("returncode") == 0:
                try:
                    row.update(numeric_memory.compare_records(expected, Path(str(prefix)+".stdout").read_bytes()))
                except ValueError as exc:
                    row.update(status="malformed_output", error=str(exc))
            evidence["omp_stack_probes"].append(row)
            (probe_dir / "results.json").write_text(json.dumps(evidence["omp_stack_probes"],indent=2)+"\n")
    if a.omp_probes:
        route = {"mi300x": "omp-hip", "h100": "omp-cuda", "chiatta": "omp-spirv"}[a.profile]
        probe_dir = ROOT / "artifacts" / a.profile / f"omp-probes-{os.environ['COBALT_JOBID']}"
        probe_dir.mkdir(parents=True, exist_ok=True)
        evidence["omp_probes"] = []
        evidence["omp_probe_source_files"] = conformance.snapshot(probe_dir / "source", "WORKTREE")
        expressions = {
            "printf": 'printf("%c%c",79,75);',
            "printf-newline": 'printf("%c%c\\n",79,75);',
            "memory": 'auto result=ffmt::format_to_n(output,64,"OK");length=result.written;',
            "print-literal": 'FFMT_PRINT("OK");',
            "print-nttp": 'ffmt::print<"OK">();',
            "transport": 'ffmt::detail::emit_chunk("OK",2);',
        }
        for opt in (0, 2):
            for name, expression in expressions.items():
                source = probe_dir / f"{name}-O{opt}.cpp"
                executable = source.with_suffix("")
                source.write_text('#include <omp.h>\n#include <cstdio>\n#include <ffmt/base.hpp>\n'
                                  'int main(){int initial=1;\n#pragma omp target map(from:initial)\n'
                                  '{initial=omp_is_initial_device();}\nif(initial)return 77;\n'
                                  'char output[64]{};std::size_t length=0;\n'
                                  '#pragma omp target map(from:output[0:64],length)\n{'+expression+'}\n'
                                  + ('std::fwrite(output,1,length,stdout);' if name == 'memory' else '')+'return 0;}\n')
                compile_command = [*conformance.compiler(route), *conformance.flags(route,opt),
                                   '-I'+str(probe_dir/'source/include'),str(source),'-o',str(executable)]
                build = run(compile_command,base,180)
                entry = dict(name=name,opt=opt,build=build)
                if build.get("returncode") == 0:
                    entry["execution"] = run([str(executable)],base)
                    expected = "OK\n" if name == "printf-newline" else "OK"
                    entry["status"] = "pass" if entry["execution"].get("returncode") == 0 and entry["execution"].get("stdout") == expected else "execution_failure"
                else:
                    entry["status"] = "compile_failure"
                evidence["omp_probes"].append(entry)
                (probe_dir/'results.json').write_text(json.dumps(evidence['omp_probes'],indent=2)+'\n')
    for path in glob.glob("/dev/dri/*"):
        stat = Path(path).stat()
        evidence["device_files"][path] = dict(mode=oct(stat.st_mode), uid=stat.st_uid, gid=stat.st_gid,
                                             readable=os.access(path, os.R_OK), writable=os.access(path, os.W_OK))
    evidence["kernel_drivers"] = {path: os.path.realpath(path) for path in glob.glob("/sys/class/drm/card*/device/driver")}
    if a.numeric_probes:
        probe_dir = ROOT / "artifacts" / a.profile / f"numeric-probes-{os.environ['COBALT_JOBID']}"
        probe_dir.mkdir(parents=True, exist_ok=True)
        evidence["numeric_probes"] = []
        evidence["numeric_probe_source_files"] = conformance.snapshot(probe_dir / "source", "WORKTREE")
        variants = [("host", [os.environ.get("FFMT_HOST_CXX", "g++")], ["-std=c++20", "-O2"], {}),
                    ("sycl-O0", conformance.compiler("sycl"), conformance.flags("sycl", 0), {}),
                    ("sycl-O2", conformance.compiler("sycl"), conformance.flags("sycl", 2), {}),
                    ("sycl-O2-no-early", conformance.compiler("sycl"), conformance.flags("sycl", 2)+["-fno-sycl-early-optimizations"], {}),
                    ("sycl-O2-no-jit-opt", conformance.compiler("sycl"), conformance.flags("sycl", 2), {"SYCL_PROGRAM_COMPILE_OPTIONS": "-cl-opt-disable"}),
                    ("sycl-O2-optnone", conformance.compiler("sycl"), conformance.flags("sycl", 2)+["-DPROBE_OPTNONE"], {})]
        expected = None
        for label, compiler, flags, extra in variants:
            executable = probe_dir / label
            flags = flags + ([] if label == "host" else ["-DPROBE_SYCL"])
            compile_command = [*compiler, *flags, "-I"+str(probe_dir/"source/include"), str(ROOT/"tools/jlse/numeric_probe.cpp"), "-o", str(executable)]
            build = run(compile_command, base, 180)
            row = dict(label=label, build=build, environment=extra)
            if build.get("returncode") == 0:
                row["execution"] = run([str(executable)], dict(base, **extra))
                if label == "host": expected = row["execution"].get("stdout")
                row["status"] = "pass" if expected is not None and row["execution"].get("returncode") == 0 and row["execution"].get("stdout") == expected else "mismatch"
            else:
                row["status"] = "compile_failure"
            evidence["numeric_probes"].append(row)
            (probe_dir/"results.json").write_text(json.dumps(evidence["numeric_probes"],indent=2)+"\n")
    for label, env in [("configured", base), ("native_driver", native), ("shared_1032", driver1032)]:
        if a.configured_only and label != "configured": continue
        commands = [[sys.executable, "-c", ZE_PROBE]]
        for name in ("sycl-ls", "clinfo"):
            path = shutil.which(name)
            if path:
                commands.append([path, "--verbose"] if name == "sycl-ls" else [path, "-l"])
        if a.executable and not a.omp_stack_probes:
            env["LIBOMPTARGET_DEBUG"] = "2"
            if a.trace_sycl: env["SYCL_UR_TRACE"] = "2"
            commands.append([a.executable, "0"])
        evidence["variants"][label] = dict(ld_library_path=env.get("LD_LIBRARY_PATH"),
                                            commands=[run(cmd, env, a.timeout if cmd[0] == a.executable else 60,
                                                          ROOT/"artifacts"/a.profile/f"trace-{os.environ['COBALT_JOBID']}-{label}" if cmd[0] == a.executable else None) for cmd in commands])
    output = ROOT / "artifacts" / a.profile / f"diagnostics-{os.environ['COBALT_JOBID']}.json"
    output.write_text(json.dumps(evidence, indent=2) + "\n")
    print(output)


if __name__ == "__main__":
    main()
