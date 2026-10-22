#!/usr/bin/env python3
"""Independent raw-byte fmt differential qualification, with no fmt device code."""
import argparse
import base64
import datetime
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tarfile
import time
import numeric_memory

ROOT = Path(__file__).resolve().parents[1]
CONFIG = json.loads((ROOT / "platforms/profiles.json").read_text())
ROUTES = ("host-buffer", "host-specifiers", "cuda", "nvcc", "hip", "omp-cuda", "omp-hip", "omp-spirv", "sycl", "acpp", "chipstar", "kokkos-serial", "kokkos-cuda", "kokkos-hip", "kokkos-sycl")
_spec = importlib.util.spec_from_file_location("ffmt_cases", str(ROOT / "test/conformance/cases.py"))
fixtures = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fixtures)


def compare_bytes(expected, actual):
    if expected == actual:
        return dict(status="pass", expected_bytes=len(expected), actual_bytes=len(actual))
    offset = next((i for i, (a, b) in enumerate(zip(expected, actual)) if a != b), min(len(expected), len(actual)))
    return dict(status="mismatch", first_difference=offset,
                symptom="truncated" if expected.startswith(actual) else "different_bytes",
                expected_bytes=len(expected), actual_bytes=len(actual),
                expected_base64=base64.b64encode(expected).decode("ascii"),
                actual_base64=base64.b64encode(actual).decode("ascii"))


def negative_status(p):
    if p["exit_code"] == 0:
        return "unexpected_acceptance"
    text = p["stderr"].decode("utf-8", "replace")
    if p["exit_code"] < 0 or re.search(
        r"internal compiler error|PLEASE submit a bug|LLVM ERROR|Segmentation fault|"
        r"No such file or directory|file not found|unable to execute|cannot find -l|"
        r"No available targets|unsupported.*(gpu|architecture|target)|failed to create.*thread",
        text, re.I):
        return "infrastructure_failure"
    if re.search(r"constant expression|consteval|constexpr|invalid.format|invalid.*(spec|type)|"
                 r"format.*(error|invalid|reject)|precision|sign_not_allowed|"
                 r"unmatched|argument_index|cannot_mix|not.allowed", text, re.I):
        return "expected_rejection"
    return "wrong_diagnostic"


def run(command, directory, stem, timeout=180):
    directory.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    try:
        with subprocess.Popen([str(x) for x in command], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, cwd=str(ROOT),
                              start_new_session=True) as p:
            try:
                stdout, stderr = p.communicate(timeout=timeout)
                result = dict(exit_code=p.returncode, stdout=stdout, stderr=stderr)
            except subprocess.TimeoutExpired:
                # Compiler drivers and JIT launchers can have descendants that
                # keep running and hold these pipes open after the parent dies.
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                stdout, stderr = p.communicate()
                result = dict(exit_code=-1, stdout=stdout, stderr=stderr, timeout=True)
    except OSError as e:
        result = dict(exit_code=-1, stdout=b"", stderr=str(e).encode())
    result.update(command=[str(x) for x in command], seconds=time.monotonic() - start)
    for stream in ("stdout", "stderr"):
        path = directory / (stem + "." + stream)
        path.write_bytes(result[stream])
        result[stream + "_file"] = str(path.relative_to(ROOT))
    return result


def record(p):
    return {k: v for k, v in p.items() if k not in ("stdout", "stderr")}


def git(*args):
    return subprocess.check_output(["git"] + list(args), cwd=str(ROOT)).decode().strip()


def snapshot(destination, revision):
    if revision == "WORKTREE":
        shutil.copytree(str(ROOT / "include"), str(destination / "include"))
    else:
        data = subprocess.check_output(["git", "archive", revision, "include"], cwd=str(ROOT))
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            for member in archive.getmembers():
                if not member.isfile():
                    continue
                name = Path(member.name)
                if name.is_absolute() or ".." in name.parts:
                    raise ValueError("unsafe source archive member")
                path = destination / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.extractfile(member).read())
    return {str(p.relative_to(destination)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((destination / "include").rglob("*")) if p.is_file()}


def compiler(route):
    defaults = {"host-buffer": os.environ.get("CXX", "g++"), "host-specifiers": os.environ.get("CXX", "g++"),
                "cuda": "clang++", "nvcc": "nvcc", "hip": "hipcc", "omp-cuda": "clang++",
                "omp-hip": "clang++", "omp-spirv": "icpx", "sycl": "icpx",
                "acpp": "acpp", "chipstar": "hipcc"}
    base = {"kokkos-serial": "host-buffer", "kokkos-cuda": "cuda", "kokkos-hip": "hip", "kokkos-sycl": "sycl"}.get(route)
    default = " ".join(compiler(base)) if base else defaults.get(route, "c++")
    return shlex.split(os.environ.get("FFMT_CXX_" + route.upper().replace("-", "_"), default))


def kokkos_prefix(route):
    key = "FFMT_KOKKOS_" + route.split("-", 1)[1].upper() + "_PREFIX"
    return Path(os.environ.get(key, ROOT / "software" / os.environ.get("FFMT_PROFILE", "local") / "install" / route))


def link_flags(route):
    if not route.startswith("kokkos-"):
        return []
    prefix = kokkos_prefix(route)
    library = prefix / "lib"
    if not library.is_dir():
        library = prefix / "lib64"
    return ["-L" + str(library), "-lkokkoscontainers", "-lkokkoscore", "-ldl", "-pthread"] + (["-lcuda"] if route == "kokkos-cuda" else [])


def flags(route, opt):
    if route.startswith("kokkos-"):
        base = {"kokkos-serial": "host-buffer", "kokkos-cuda": "cuda", "kokkos-hip": "hip", "kokkos-sycl": "sycl"}[route]
        return flags(base, opt) + ["-I" + str(kokkos_prefix(route) / "include")] + shlex.split(os.environ.get("FFMT_FLAGS_" + route.upper().replace("-", "_"), ""))
    cuda = os.environ.get("CUDA_PATH", "/usr/local/cuda")
    rocm = os.environ.get("ROCM_PATH", "/opt/rocm")
    arch = os.environ.get("FFMT_GPU_ARCH")
    if not arch and route in ("cuda", "nvcc", "omp-cuda"):
        try:
            cap = subprocess.check_output(["nvidia-smi", "--query-gpu=compute_cap", "--format=csv,noheader"], timeout=15)
            arch = "sm_" + cap.decode().splitlines()[0].strip().replace(".", "")
        except (OSError, subprocess.SubprocessError, IndexError):
            arch = "sm_90"
    if not arch and route in ("hip", "omp-hip"):
        try:
            arch = subprocess.check_output([str(Path(rocm) / "llvm/bin/amdgpu-arch")], timeout=15).decode().splitlines()[0]
        except (OSError, subprocess.SubprocessError, IndexError):
            arch = "gfx942"
    routes = {
        "host-buffer": ["-DFFMT_BUFFER_PATH=1"], "host-specifiers": ["-DFFMT_BUFFER_PATH=0"],
        "cuda": ["-x", "cuda", "--cuda-path=" + cuda, "--cuda-gpu-arch=" + (arch or "sm_90"), "-L" + cuda + "/lib64", "-lcudart"],
        "nvcc": ["-x", "cu", "--extended-lambda", "--expt-relaxed-constexpr", "-arch=" + (arch or "sm_90"), "-Xcompiler=-fno-fast-math"],
        "hip": ["-x", "hip", "--offload-arch=" + (arch or "gfx942")],
        "omp-cuda": ["-fopenmp", "--offload-arch=" + (arch or "sm_90"), "--cuda-path=" + cuda],
        "omp-hip": ["-fopenmp", "--offload-arch=" + (arch or "gfx942"), "--rocm-path=" + rocm],
        "sycl": ["-fsycl"], "omp-spirv": ["-fiopenmp", "-fopenmp-targets=spir64"],
        "acpp": ["--acpp-targets=generic"], "chipstar": ["-x", "hip"]}
    extra = shlex.split(os.environ.get("FFMT_FLAGS_" + route.upper().replace("-", "_"), ""))
    return ["-std=c++20", "-O" + str(opt)] + ([] if route == "nvcc" else ["-fno-fast-math"]) + routes.get(route, []) + extra


def expression(item, reference=False):
    special = item.get("oracle_expr" if reference else "ffmt_expr")
    if special:
        return special
    args = ", ".join([json.dumps(item["pattern"], ensure_ascii=False)] + item["args"])
    if reference:
        return "fmt::format(" + args + ")" + (' + "\\n"' if item.get("println") else "")
    return ("FFMT_PRINTLN(" if item.get("println") else "FFMT_PRINT(") + args + ")"


def extra_headers(items, reference=False):
    keys = ("oracle_includes", "oracle_preamble") if reference else ("includes", "preamble")
    headers = sorted({p for item in items for p in item.get(keys[0], [])})
    return "\n".join("#include <" + p + ">" for p in headers) + "\n" + "\n".join(dict.fromkeys(item.get(keys[1], "") for item in items))


PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
template<class T, class U> HD T bits(U v) {return __builtin_bit_cast(T, v);}
"""


def oracle_source(items):
    body = "\n".join("case %d: {auto s=%s; std::fwrite(s.data(),1,s.size(),stdout); return 0;}" %
                     (i, expression(item, True)) for i, item in enumerate(items))
    return "#define HD\n#include <fmt/format.h>\n" + PRELUDE + extra_headers(items, True) + """
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  switch(std::atoi(argv[1])) {""" + body + "}return 2;}\n"


def target_source(items, indices, route):
    if route not in ROUTES:
        raise ValueError("unknown backend route: " + route)
    kokkos = route.startswith("kokkos-")
    cuda, hip, sycl = route in ("cuda", "nvcc", "kokkos-cuda"), route in ("hip", "chipstar", "kokkos-hip"), route in ("sycl", "acpp", "kokkos-sycl")
    omp = route.startswith("omp-")
    headers = "#include <cuda_runtime.h>\n" if cuda else "#include <hip/hip_runtime.h>\n" if hip else "#include <sycl/sycl.hpp>\n" if sycl else "#include <omp.h>\n" if omp else ""
    body = "\n".join("case %d: {%s; break;}" % (i, expression(items[i])) for i in indices)
    source = headers + ("#include <Kokkos_Core.hpp>\n" if kokkos else "") + "#define HD " + ("__host__ __device__" if cuda or hip else "") + "\n#include <ffmt/base.hpp>\n" + PRELUDE
    source += extra_headers([items[i] for i in indices]) + "\nHD void execute_case(int index) {switch(index) {" + body + "}}\n"
    if kokkos:
        expected_space = {"kokkos-serial": "Serial", "kokkos-cuda": "Cuda", "kokkos-hip": "HIP", "kokkos-sycl": "SYCL"}[route]
        source += r'''
#include <cstring>
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  Kokkos::initialize(argc,argv);
  if(std::strcmp(Kokkos::DefaultExecutionSpace::name(),"EXPECTED_SPACE")!=0){Kokkos::finalize();return 77;}
  KOKKOS_DEVICE_GUARD
  int index=std::atoi(argv[1]);
  Kokkos::parallel_for("ffmt-case",Kokkos::RangePolicy<>(0,1),KOKKOS_LAMBDA(int){execute_case(index);});
  Kokkos::fence();Kokkos::finalize();return 0;
}
'''.replace("EXPECTED_SPACE", expected_space).replace("KOKKOS_DEVICE_GUARD", numeric_memory.kokkos_device_guard(route))
    elif cuda or hip:
        prefix = "cuda" if cuda else "hip"
        source += "__global__ void kernel(int index) {execute_case(index);}\n"
        source += r"""
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  int n=0; if(PREFIXGetDeviceCount(&n)!=PREFIXSuccess || n==0) return 77;
  PROPTYPE p{}; if(PREFIXGetDeviceProperties(&p,0)!=PREFIXSuccess) return 78;
  std::fprintf(stderr,"FFMT_DEVICE KIND name=%s arch=%d.%d\n",p.name,p.major,p.minor);
  LAUNCH;
  if(PREFIXGetLastError()!=PREFIXSuccess) return 79;
  if(PREFIXDeviceSynchronize()!=PREFIXSuccess) return 80;
  return 0;
}
""".replace("PREFIX", prefix).replace("PROPTYPE", "cudaDeviceProp" if cuda else "hipDeviceProp_t").replace("KIND", prefix).replace("LAUNCH", "kernel<<<1,1>>>(std::atoi(argv[1]))" if cuda else "hipLaunchKernelGGL(kernel,dim3(1),dim3(1),0,0,std::atoi(argv[1]))")
    elif sycl:
        source += r"""
#include <exception>
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  try {
    sycl::queue q{sycl::gpu_selector_v};
    auto d=q.get_device();
    auto name=d.get_info<sycl::info::device::name>();
    std::fprintf(stderr,"FFMT_DEVICE sycl name=%s fp64=%d\n",name.c_str(),d.has(sycl::aspect::fp64));
    int index=std::atoi(argv[1]);
    q.single_task([=] {execute_case(index);}).wait_and_throw();
    return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what()); return 80;}
}
"""
    elif omp:
        source += r"""
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  int initial=1;
  #pragma omp target map(from: initial)
  {initial=omp_is_initial_device();}
  if(initial) return 77;
  std::fprintf(stderr,"FFMT_DEVICE openmp device=%d initial_device=false\n",omp_get_default_device());
  int index=std::atoi(argv[1]);
  #pragma omp target firstprivate(index)
  {execute_case(index);}
  return 0;
}
"""
    else:
        source += r"""
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  std::fprintf(stderr,"FFMT_DEVICE host\n");
  execute_case(std::atoi(argv[1])); return 0;
}
"""
    return source


class Campaign:
    def __init__(self, args):
        self.args = args
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        self.output = ROOT / args.output if args.output else ROOT / "artifacts" / args.profile / (args.label + "-" + stamp)
        self.output.mkdir(parents=True, exist_ok=False)
        hashes = snapshot(self.output / "source", args.revision)
        self.host_cxx = shlex.split(args.host_cxx or os.environ.get("FFMT_HOST_CXX", "g++"))
        self.fmt = Path(args.fmt_dir).resolve()
        self.manifest = dict(schema_version=1, profile=args.profile, label=args.label, started=stamp,
            source=dict(commit=git("rev-parse", "HEAD" if args.revision == "WORKTREE" else args.revision),
                        worktree=args.revision == "WORKTREE", dirty=bool(git("status", "--porcelain")), files=hashes),
            oracle=dict(repository="https://github.com/fmtlib/fmt", commit=CONFIG["fmt_commit"]),
            host=dict(name=platform.node(), architecture=platform.machine(), os=platform.platform()),
            environment={k: v for k, v in os.environ.items() if k in ("LC_ALL", "OMP_TARGET_OFFLOAD", "ONEAPI_DEVICE_SELECTOR", "ACPP_VISIBILITY_MASK", "CHIP_BE", "CHIP_DEVICE_TYPE", "CHIP_SKIP_UNINIT")},
            configurations=[])
        self.manifest["corpus_sha256"] = hashlib.sha256((ROOT / "test/conformance/cases.py").read_bytes()).hexdigest()
        self.manifest["selection"] = dict(
            case_ids=[x["id"] for x in fixtures.CASES if not args.case or x["id"] in args.case],
            routes=args.route or CONFIG["profiles"][args.profile]["routes"],
            optimizations=args.opt or [0, 2], numeric_samples=args.numeric_samples,
            memory=not args.no_memory, negative=not args.no_negative, shard_size=args.shard_size)
        self.manifest["runner_files"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (ROOT / "tools/conformance.py", ROOT / "tools/numeric_memory.py", ROOT / "test/conformance/cases.py")}
        self.manifest["timeouts"] = dict(compile=args.compile_timeout, execution=args.run_timeout, cold_jit=args.jit_timeout)
        self.manifest["cobalt_jobid"] = os.environ.get("COBALT_JOBID")
        discovery = ROOT / "artifacts" / args.profile / ("discovery-" + str(self.manifest["cobalt_jobid"]) + ".json")
        if discovery.is_file():
            self.manifest["discovery"] = dict(path=str(discovery.relative_to(ROOT)),
                sha256=hashlib.sha256(discovery.read_bytes()).hexdigest(),
                evidence=json.loads(discovery.read_text()))
        if git("-C", str(self.fmt), "rev-parse", "HEAD") != CONFIG["fmt_commit"]:
            raise ValueError("fmt checkout differs from oracle pin")
        self.reference = self.output / "oracle"
        self.reference.mkdir()
        self.save()

    def save(self):
        counts = {}
        for config in self.manifest["configurations"]:
            for item in config.get("cases", []) + config.get("negative_cases", []):
                counts[item["status"]] = counts.get(item["status"], 0) + 1
        self.manifest["counts"] = counts
        path = self.output / "manifest.json"
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps(self.manifest, indent=2, sort_keys=True) + "\n")
        temporary.replace(path)

    def build_oracle(self, items):
        common = self.host_cxx + ["-std=c++20", "-O2", "-fno-fast-math", "-I" + str(self.fmt / "include")]
        obj = self.reference / "fmt.o"
        p = run(common + ["-c", self.fmt / "src/format.cc", "-o", obj], self.reference, "fmt-build", self.args.compile_timeout)
        self.manifest["oracle"]["build"] = record(p)
        if p["exit_code"]:
            raise RuntimeError("oracle object failed: " + p["stderr_file"])
        source = self.reference / "cases.cpp"
        source.write_text(oracle_source(items))
        self.manifest["oracle"]["cases_source_sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
        p = run(common + [source, obj, "-o", self.reference / "cases"], self.reference, "oracle-build", self.args.compile_timeout)
        self.manifest["oracle"]["cases_build"] = record(p)
        if p["exit_code"]:
            raise RuntimeError("oracle cases failed: " + p["stderr_file"])
        expected = []
        for i, item in enumerate(items):
            p = run([self.reference / "cases", i], self.reference, item["id"], self.args.run_timeout)
            if p["exit_code"]:
                raise RuntimeError("oracle execution failed: " + item["id"])
            expected.append(p["stdout"])
        self.save()
        return expected

    def configuration(self, route, opt, items, expected):
        directory = self.output / (route + "-O" + str(opt))
        directory.mkdir()
        cmd = compiler(route) + flags(route, opt) + ["-I" + str(self.output / "source/include")]
        config = dict(route=route, optimization=opt, command_prefix=cmd, cases=[], negative_cases=[])
        execution_timeout = max(self.args.run_timeout, self.args.jit_timeout) if route in ("sycl", "acpp", "chipstar", "omp-spirv", "kokkos-sycl") else self.args.run_timeout
        config["execution_timeout"] = execution_timeout
        self.manifest["configurations"].append(config)
        config["compiler_version"] = record(run(compiler(route) + ["--version"], directory, "compiler-version"))
        control = [dict(id="control", pattern="control {}", args=["7"])]
        src = directory / "control.cpp"
        src.write_text(target_source(control, [0], route))
        p = run(cmd + [src] + link_flags(route) + ["-o", directory / "control"], directory, "control-build", self.args.compile_timeout)
        config["control_build"] = record(p)
        if p["exit_code"]:
            config.update(stdout_status="control_compile_failure", cases=[dict(id=x["id"], family=x["family"], status="not_run", reason="stdout control did not compile") for x in items])
            self.save()
            if not self.args.no_memory and (self.output / "source/include/ffmt/core.hpp").is_file():
                self.memory_suite(cmd, directory, route, config)
            config["status"] = config["stdout_status"]
            self.save()
            print("%s -O%s: control compile failed (%s)" % (route, opt, p["stderr_file"]), flush=True)
            return
        p = run([directory / "control", 0], directory, "control-run", execution_timeout)
        config["control_run"] = record(p)
        metadata = [line for line in p["stderr"].decode("utf-8", "replace").splitlines() if line.startswith("FFMT_DEVICE ")]
        config["device"] = metadata[0][12:] if metadata else None
        if p["exit_code"] or p["stdout"] != b"control 7" or not metadata:
            config.update(stdout_status="control_execution_failure", cases=[dict(id=x["id"], family=x["family"], status="not_run", reason="stdout control did not execute correctly") for x in items])
            self.save()
            if not self.args.no_memory and (self.output / "source/include/ffmt/core.hpp").is_file():
                self.memory_suite(cmd, directory, route, config)
            config["status"] = config["stdout_status"]
            self.save()
            print("%s -O%s: execution control failed" % (route, opt), flush=True)
            return

        def shard(indices):
            label = "cases-%d-%d" % (indices[0], indices[-1])
            src = directory / (label + ".cpp")
            src.write_text(target_source(items, indices, route))
            p = run(cmd + [src] + link_flags(route) + ["-o", directory / label], directory, label + "-build", self.args.compile_timeout)
            if p["exit_code"] and len(indices) > 1:
                config.setdefault("shard_failures", []).append(dict(
                    stage="build", case_ids=[items[i]["id"] for i in indices],
                    source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(), build=record(p),
                    status="compile_timeout" if p.get("timeout") else "compile_failure"))
                self.save()
                mid = len(indices) // 2
                shard(indices[:mid])
                shard(indices[mid:])
                return
            for position, i in enumerate(indices):
                item = items[i]
                result = dict(id=item["id"], family=item["family"], build=record(p))
                result["source_sha256"] = hashlib.sha256(src.read_bytes()).hexdigest()
                if p["exit_code"]:
                    text = p["stderr"].decode("utf-8", "replace")
                    result["status"] = "unsupported" if re.search(r"cannot express with printf|format string too long|too_many_placeholders|incompatible|invalid_format_spec|invalid_placeholder", text) and negative_status(p) != "infrastructure_failure" else "compile_failure"
                else:
                    execution = run([directory / label, i], directory, item["id"], execution_timeout)
                    result["execution"] = record(execution)
                    result.update(dict(status="execution_timeout" if execution.get("timeout") else "execution_failure") if execution["exit_code"] else compare_bytes(expected[i], execution["stdout"]))
                if result["status"] != "pass" and "execution" in result and len(indices) > 1:
                    # A large switch can change device stack/code generation.
                    # Preserve the failed binary evidence, then isolate every
                    # remaining case before assigning its final outcome.
                    config.setdefault("shard_failures", []).append(result)
                    self.save()
                    remaining = indices[position:]
                    print("%s -O%s %s: isolating %s" % (route, opt, item["id"], result["status"]), flush=True)
                    if len(remaining) == 1:
                        shard(remaining)
                    else:
                        mid = len(remaining) // 2
                        shard(remaining[:mid])
                        shard(remaining[mid:])
                    return
                config["cases"].append(result)
                print("%s -O%s %s: %s" % (route, opt, item["id"], result["status"]), flush=True)
                self.save()

        applicable = []
        for index, item in enumerate(items):
            missing = [header for header in item.get("requires", []) if not (self.output / "source/include" / header).is_file()]
            if missing:
                config["cases"].append(dict(id=item["id"], family=item["family"], status="unsupported", reason="source snapshot lacks " + ", ".join(missing)))
            else:
                applicable.append(index)
        for first in range(0, len(applicable), self.args.shard_size):
            shard(applicable[first:first + self.args.shard_size])
        config["stdout_status"] = "completed"
        self.save()
        if not self.args.no_memory and (self.output / "source/include/ffmt/core.hpp").is_file():
            self.memory_suite(cmd, directory, route, config)
        if not self.args.no_negative:
            self.negatives(cmd, directory, route, config)
        config["status"] = config["stdout_status"]
        self.save()

    def memory_suite(self, cmd, directory, route, config):
        src = directory / "numeric-memory.cpp"
        src.write_text(numeric_memory.source(route, samples=self.args.numeric_samples))
        p = run(cmd + [src] + link_flags(route) + ["-o", directory / "numeric-memory"], directory, "numeric-memory-build", self.args.compile_timeout)
        result = dict(build=record(p), status="compile_failure", source_sha256=hashlib.sha256(src.read_bytes()).hexdigest())
        config["numeric_memory"] = result
        if p["exit_code"]:
            return
        if not (self.reference / "numeric-memory").exists():
            src = self.reference / "numeric-memory.cpp"
            src.write_text(numeric_memory.source("host-buffer", oracle=True, samples=self.args.numeric_samples))
            reference_build = run(self.host_cxx + ["-std=c++20", "-O2", "-fno-fast-math", "-I" + str(self.fmt / "include"), src, self.reference / "fmt.o", "-o", self.reference / "numeric-memory"], self.reference, "numeric-memory-build", self.args.compile_timeout)
            self.manifest["oracle"]["numeric_memory_build"] = record(reference_build)
            if reference_build["exit_code"]:
                raise RuntimeError("numeric oracle build failed")
            reference_run = run([self.reference / "numeric-memory"], self.reference, "numeric-memory-run", self.args.run_timeout)
            self.manifest["oracle"]["numeric_memory_run"] = record(reference_run)
            if reference_run["exit_code"]:
                raise RuntimeError("numeric oracle execution failed")
        expected = (self.reference / "numeric-memory-run.stdout").read_bytes()
        execution = run([directory / "numeric-memory"], directory, "numeric-memory-run", max(config.get("execution_timeout", self.args.run_timeout), 120))
        result["execution"] = record(execution)
        if execution["exit_code"]:
            result["status"] = "execution_timeout" if execution.get("timeout") else "execution_failure"
        else:
            try:
                result.update(numeric_memory.compare_records(expected, execution["stdout"]))
            except ValueError as error:
                result.update(status="transport_failure", reason=str(error))
        print("%s numeric memory: %s (%s passed)" % (route, result["status"], result.get("passed", 0)), flush=True)

    def negatives(self, cmd, directory, route, config):
        for item in fixtures.NEGATIVE_CASES:
            src = directory / ("negative-" + item["id"] + ".cpp")
            src.write_text(target_source([item], [0], route))
            p = run(cmd + ["-c", src, "-o", directory / "negative.o"], directory, "negative-" + item["id"], self.args.compile_timeout)
            config["negative_cases"].append(dict(id=item["id"], family=item["family"], status=negative_status(p), build=record(p)))
        if "negative_cases" not in self.manifest["oracle"]:
            self.manifest["oracle"]["negative_cases"] = []
            for item in fixtures.NEGATIVE_CASES:
                src = self.reference / ("negative-" + item["id"] + ".cpp")
                src.write_text(oracle_source([item]))
                p = run(self.host_cxx + ["-std=c++20", "-I" + str(self.fmt / "include"), "-c", src, "-o", self.reference / "negative.o"], self.reference, "negative-" + item["id"], self.args.compile_timeout)
                self.manifest["oracle"]["negative_cases"].append(dict(id=item["id"], status=negative_status(p), build=record(p)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=sorted(CONFIG["profiles"]), default="local")
    parser.add_argument("--route", action="append", choices=ROUTES)
    parser.add_argument("--opt", type=int, choices=(0, 1, 2, 3), action="append")
    parser.add_argument("--revision", default="WORKTREE")
    parser.add_argument("--label", default="head")
    parser.add_argument("--output")
    parser.add_argument("--fmt-dir", default=str(ROOT / "third_party/fmt"))
    parser.add_argument("--host-cxx")
    parser.add_argument("--case", action="append", choices=sorted(x["id"] for x in fixtures.CASES))
    parser.add_argument("--shard-size", type=int, default=16)
    parser.add_argument("--compile-timeout", type=int, default=180)
    parser.add_argument("--run-timeout", type=int, default=30)
    parser.add_argument("--jit-timeout", type=int, default=600, help="execution cap for routes that compile device images at launch")
    parser.add_argument("--no-negative", action="store_true")
    parser.add_argument("--no-memory", action="store_true")
    parser.add_argument("--numeric-samples", type=int, default=4096)
    args = parser.parse_args()
    if args.shard_size < 1:
        parser.error("--shard-size must be positive")
    if args.numeric_samples < 0:
        parser.error("--numeric-samples must be nonnegative")
    os.environ["LC_ALL"] = "C"
    os.environ["OMP_TARGET_OFFLOAD"] = "MANDATORY"
    os.environ["FFMT_PROFILE"] = args.profile
    items = [x for x in fixtures.CASES if not args.case or x["id"] in args.case]
    if not items:
        parser.error("no cases selected")
    campaign = Campaign(args)
    try:
        expected = campaign.build_oracle(items)
        for route in args.route or CONFIG["profiles"][args.profile]["routes"]:
            for opt in args.opt or (0, 2):
                campaign.configuration(route, opt, items, expected)
        campaign.manifest["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    except Exception as e:
        campaign.manifest["fatal_error"] = str(e)
        raise
    finally:
        campaign.save()
        print("Manifest: " + str(campaign.output / "manifest.json"), flush=True)
    bad = any(status not in ("pass", "expected_rejection") for status in campaign.manifest["counts"])
    bad |= any(c.get("numeric_memory", {}).get("status", "pass") != "pass" for c in campaign.manifest["configurations"])
    bad |= any(x["status"] != "expected_rejection" for x in campaign.manifest["oracle"].get("negative_cases", []))
    return int(bad)


if __name__ == "__main__":
    sys.exit(main())
