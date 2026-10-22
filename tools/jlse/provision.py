#!/usr/bin/env python3
"""Install pinned dependencies into an ignored system prefix, inside a job."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import tarfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
RECIPES = json.loads((ROOT / "platforms/dependencies.json").read_text())


def run(argv, log, cwd=None):
    print(shlex.join(map(str, argv)), flush=True)
    start = time.monotonic()
    result = subprocess.run(list(map(str, argv)), cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {shlex.join(map(str, argv))}")
    return time.monotonic() - start


def source(name, base, log):
    recipe = RECIPES["sources"][name]
    src = base / "src" / f"{name}-{recipe['tag']}"
    if not (src / ".git").exists():
        run(["git", "clone", "--depth", "1", "--branch", recipe["tag"], recipe["repository"], src], log)
    commit = subprocess.check_output(["git", "-C", src, "rev-parse", "HEAD"], text=True).strip()
    if commit != recipe["commit"]:
        raise RuntimeError(f"{name}: expected {recipe['commit']}, found {commit}")
    return src


def install_archive(name, base, log):
    recipe = RECIPES["archives"][name]
    if recipe["architecture"] != platform.machine():
        raise RuntimeError(f"{name}: archive is for {recipe['architecture']}; host is {platform.machine()}")
    archive = base / "downloads" / recipe["url"].rsplit("/", 1)[-1]
    if not archive.exists():
        temporary = archive.with_suffix(".partial")
        with urllib.request.urlopen(recipe["url"], timeout=120) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        temporary.replace(archive)
    digest = hashlib.sha256()
    with archive.open("rb") as f:
        for chunk in iter(lambda: f.read(8 * 1024 * 1024), b""):
            digest.update(chunk)
    if digest.hexdigest() != recipe["sha256"]:
        raise RuntimeError(f"{archive}: SHA-256 mismatch")
    prefix = base / "install" / recipe.get("install_name", name)
    prefix.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as package:
        members = package.getmembers()
        tops = {Path(m.name).parts[0] for m in members if Path(m.name).parts and m.name != "."}
        single_top = len(tops) == 1 and not (tops & {"bin", "lib", "include"})
        for m in members:
            parts = Path(m.name).parts[1:] if single_top else Path(m.name).parts
            if not parts:
                continue
            if Path(m.name).is_absolute() or ".." in parts:
                raise RuntimeError(f"Unsafe archive member: {m.name}")
            m.name = str(Path(*parts))
            package.extract(m, prefix)
    return {"archive": str(archive), "sha256": digest.hexdigest(), "prefix": str(prefix)}


def cmake_install(name, src, base, flags, jobs, log):
    build = base / "build" / name
    prefix = base / "install" / name
    flags = [f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_BUILD_TYPE=Release", *flags]
    configure_seconds = run(["cmake", "-S", src, "-B", build, *flags], log)
    build_seconds = run(["cmake", "--build", build, "--parallel", jobs], log)
    install_seconds = run(["cmake", "--install", build], log)
    return {"prefix": str(prefix), "flags": flags, "configure_seconds": configure_seconds,
            "build_seconds": build_seconds, "install_seconds": install_seconds}


def provision(component, args, base, log):
    if component == "intel-llvm":
        return install_archive(component, base, log)
    name = "kokkos" if component.startswith("kokkos-") else component
    src = source(name, base, log)
    if args.fetch_only:
        return {"source": str(src), "commit": RECIPES["sources"][name]["commit"], "status": "source-ready"}
    flags = []
    compiler = os.getenv("FFMT_HOST_CXX", shutil.which("g++") or "g++")
    if name == "adaptivecpp":
        llvm = Path(args.llvm_prefix or "/soft/compilers/llvm/release-20.1.8")
        if not any((llvm / "lib").glob("libLLVM.so*")) and not args.llvm_prefix:
            local_llvm = base / "install/llvm20"
            if not (local_llvm / "lib/libLLVM.so").exists():
                install_archive("llvm20", base, log)
            llvm = local_llvm
        flags += [f"-DCMAKE_C_COMPILER={llvm}/bin/clang", f"-DCMAKE_CXX_COMPILER={llvm}/bin/clang++",
                  f"-DLLVM_DIR={llvm}/lib/cmake/llvm", "-DACPP_COMPILER_FEATURE_PROFILE=full",
                  "-DWITH_OPENCL_BACKEND=OFF", "-DWITH_LEVEL_ZERO_BACKEND=OFF",
                  "-DWITH_CUDA_BACKEND=OFF", "-DWITH_ROCM_BACKEND=OFF", "-DBUILD_TESTING=OFF"]
        if args.profile in {"h100", "gh200"}:
            flags += ["-DWITH_CUDA_BACKEND=ON", f"-DCUDA_TOOLKIT_ROOT_DIR={os.getenv('CUDA_PATH', '/usr/local/cuda')}"]
        elif args.profile == "mi300x":
            flags += ["-DWITH_ROCM_BACKEND=ON", f"-DROCM_PATH={os.getenv('ROCM_PATH', '/soft/compilers/rocm/rocm-7.0.2')}"]
        elif args.profile == "chiatta":
            flags += ["-DWITH_LEVEL_ZERO_BACKEND=ON"]
    elif name == "chipstar":
        llvm = Path(args.llvm_prefix or "/soft/compilers/llvm/release-22.1.8")
        run(["git", "-C", src, "submodule", "update", "--init", "--recursive", "--depth", "1"], log)
        flags += [f"-DLLVM_CONFIG_BIN={llvm}/bin/llvm-config", "-DCHIP_LLVM_USE_INTERGRATED_SPIRV=ON",
                  "-DCHIP_BUILD_TESTS=OFF", "-DCHIP_BUILD_SAMPLES=OFF"]
    else:
        flags += [f"-DCMAKE_CXX_COMPILER={compiler}", "-DKokkos_ENABLE_SERIAL=ON", "-DKokkos_ENABLE_TESTS=OFF",
                  "-DKokkos_ENABLE_EXAMPLES=OFF", "-DCMAKE_CXX_STANDARD=20"]
        backend = component.split("-", 1)[1]
        if backend == "cuda":
            flags += [f"-DCMAKE_CXX_COMPILER={src}/bin/nvcc_wrapper", "-DKokkos_ENABLE_CUDA=ON", "-DKokkos_ARCH_HOPPER90=ON"]
        elif backend == "hip":
            flags += [f"-DCMAKE_CXX_COMPILER={shutil.which('hipcc') or 'hipcc'}", "-DKokkos_ENABLE_HIP=ON", "-DKokkos_ARCH_AMD_GFX942=ON"]
        elif backend == "sycl":
            flags += [f"-DCMAKE_CXX_COMPILER={os.getenv('FFMT_CXX_SYCL', 'icpx')}", "-DKokkos_ENABLE_SYCL=ON",
                      "-DKokkos_ARCH_INTEL_PVC=ON"]
    if name == "chipstar":
        # Upstream configuration imports PyYAML even when its tests are off.
        python = os.getenv("FFMT_PYTHON", shutil.which("python3") or "python3")
        python_prefix = base / "install/python-deps"
        if not (python_prefix / "yaml").exists():
            run([python, "-m", "pip", "install", "--disable-pip-version-check", "--target", python_prefix, "PyYAML==6.0.3"], log)
        os.environ["PYTHONPATH"] = str(python_prefix) + os.pathsep + os.environ.get("PYTHONPATH", "")
        flags += [f"-DPython_EXECUTABLE={python}", f"-DPython3_EXECUTABLE={python}"]
    return cmake_install(component, src, base, flags, args.jobs, log)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", required=True)
    p.add_argument("--component", action="append", required=True, choices=["intel-llvm", "adaptivecpp", "chipstar",
                   "kokkos-serial", "kokkos-cuda", "kokkos-hip", "kokkos-sycl"])
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--llvm-prefix")
    p.add_argument("--fetch-only", action="store_true", help="Download sources without compilation (login-safe)")
    a = p.parse_args()
    if not a.fetch_only and not os.getenv("COBALT_JOBID"):
        p.error("Build/install requires an active Cobalt allocation")
    base = ROOT / "software" / a.profile
    for path in ["downloads", "src", "build", "install"]:
        (base / path).mkdir(parents=True, exist_ok=True)
    output = ROOT / "artifacts" / a.profile / f"provision-{os.getenv('COBALT_JOBID', 'fetch')}"
    output.mkdir(parents=True, exist_ok=True)
    results = {"schema_version": 1, "profile": a.profile, "machine": platform.machine(),
               "cobalt_jobid": os.getenv("COBALT_JOBID"), "components": {},
               "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat()}
    failed = False
    for component in a.component:
        with (output / f"{component}.log").open("w") as log:
            try:
                result = provision(component, a, base, log)
                result.setdefault("status", "installed")
            except Exception as exc:
                result = {"status": "infrastructure_failure", "error": str(exc)}
                failed = True
            results["components"][component] = result
            (output / "manifest.json").write_text(json.dumps(results, indent=2) + "\n")
            print(component, result["status"], result.get("error", ""), flush=True)
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
