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
        run(["git", "clone", "--depth", "1", *(["--filter=blob:none", "--sparse"] if name == "llvm20" else []),
             "--branch", recipe["tag"], recipe["repository"], src], log)
    if name == "llvm20":
        run(["git", "-C", src, "sparse-checkout", "set", "llvm", "clang", "lld", "cmake", "openmp", "runtimes", "compiler-rt", "libunwind"], log)
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


def cmake_install(name, src, base, flags, jobs, log, build_override=None):
    prefix = base / "install" / name
    flags = [f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_BUILD_TYPE=Release", *flags]
    source_commit = subprocess.check_output(["git", "-C", src, "rev-parse", "HEAD"], universal_newlines=True).strip()
    source_diff = subprocess.check_output(["git", "-C", src, "diff", "--binary"])
    patch_hash = hashlib.sha256(source_diff).hexdigest()
    signature = hashlib.sha256(json.dumps([str(src), source_commit, patch_hash, flags], sort_keys=True).encode()).hexdigest()[:12]
    build = base / "build" / (build_override or (name + "-" + signature))
    configure_seconds = run(["cmake", "-S", src, "-B", build, *flags], log)
    build_seconds = run(["cmake", "--build", build, "--parallel", jobs], log)
    install_seconds = run(["cmake", "--install", build, "--prefix", prefix], log)
    external = {}
    for repository in build.glob("**/.git"):
        if repository.is_dir():
            external[str(repository.parent.relative_to(build))] = subprocess.check_output(["git", "-C", repository.parent, "rev-parse", "HEAD"], universal_newlines=True).strip()
    return {"prefix": str(prefix), "build_directory": str(build), "source_commit": source_commit,
            "source_patch_sha256": patch_hash,
            "external_commits": external, "flags": flags, "configure_seconds": configure_seconds,
            "build_seconds": build_seconds, "install_seconds": install_seconds}


def typed_spirv_adapter(base, translator, major):
    """Keep LLVM HIPSPV's +all setting compatible with the measured Intel IGC."""
    wrapper = base / "install/spirv-translator-typed/bin" / f"llvm-spirv-{major}"
    wrapper.parent.mkdir(parents=True, exist_ok=True)
    wrapper.write_text(
        "#!/usr/bin/python3\nimport os,sys\ntranslator=" + repr(str(translator.resolve())) + "\n"
        'args=[a+",-SPV_KHR_untyped_pointers" if a.startswith("--spirv-ext=") else a for a in sys.argv[1:]]\n'
        "os.execv(translator,[translator,*args])\n")
    wrapper.chmod(0o755)
    metadata = dict(path=str(wrapper), sha256=hashlib.sha256(wrapper.read_bytes()).hexdigest(),
                    translator=str(translator), translator_sha256=hashlib.sha256(translator.read_bytes()).hexdigest(),
                    excluded_extension="SPV_KHR_untyped_pointers")
    (wrapper.parent.parent / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return metadata


def provision(component, args, base, log):
    if component == "intel-llvm":
        return install_archive(component, base, log)
    name = "kokkos" if component.startswith("kokkos-") else "llvm20" if component == "llvm-shared" else component
    src = source(name, base, log)
    if args.fetch_only:
        return {"source": str(src), "commit": RECIPES["sources"][name]["commit"], "status": "source-ready"}
    flags = []
    extra = {}
    compiler = os.getenv("FFMT_HOST_CXX", shutil.which("g++") or "g++")
    if name == "llvm20":
        compiler_prefix = "/soft/compilers/llvm/release-22.1.8"
        flags += [f"-DCMAKE_C_COMPILER={compiler_prefix}/bin/clang", f"-DCMAKE_CXX_COMPILER={compiler_prefix}/bin/clang++",
                  "-DLLVM_ENABLE_PROJECTS=clang;lld;openmp", "-DLLVM_ENABLE_RUNTIMES=compiler-rt",
                  "-DCOMPILER_RT_BUILD_BUILTINS=ON", "-DCOMPILER_RT_BUILD_SANITIZERS=OFF",
                  "-DCOMPILER_RT_BUILD_XRAY=OFF", "-DCOMPILER_RT_BUILD_LIBFUZZER=OFF",
                  "-DCOMPILER_RT_BUILD_PROFILE=OFF", "-DCOMPILER_RT_BUILD_ORC=OFF",
                  "-DLLVM_TARGETS_TO_BUILD=X86;NVPTX;AMDGPU;SPIRV",
                  "-DLLVM_BUILD_LLVM_DYLIB=ON", "-DLLVM_LINK_LLVM_DYLIB=ON", "-DCLANG_LINK_CLANG_DYLIB=ON",
                  "-DLLVM_USE_LINKER=lld", "-DLLVM_INCLUDE_TESTS=OFF", "-DLLVM_INCLUDE_BENCHMARKS=OFF",
                  "-DLLVM_PARALLEL_LINK_JOBS=2"]
        return cmake_install("llvm20-shared", src / "llvm", base, flags, args.jobs, log, args.resume_build)
    if name == "adaptivecpp":
        llvm = Path(args.llvm_prefix or "/soft/compilers/llvm/release-20.1.8")
        if not any((llvm / "lib").glob("libLLVM.so*")) and not args.llvm_prefix:
            local_llvm = base / "install/llvm20-shared"
            if not (local_llvm / "lib/libLLVM.so").exists():
                provision("llvm-shared", args, base, log)
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
        llvm = Path(args.llvm_prefix or base / "install/llvm20-shared")
        if not (llvm / "bin/llvm-config").exists() and not args.llvm_prefix:
            provision("llvm-shared", args, base, log)
        os.environ["LD_LIBRARY_PATH"] = str(base/"install/spirv-translator/lib") + os.pathsep + str(llvm/"lib") + os.pathsep + os.environ.get("LD_LIBRARY_PATH", "")
        run(["git", "-C", src, "submodule", "update", "--init", "--recursive", "--depth", "1"], log)
        # Upstream v1.3 fetches SPIRV-Tools main. Freeze the measured transitive
        # dependency and fingerprint this deterministic local source patch.
        cmake = src / "CMakeLists.txt"
        text = cmake.read_text()
        upstream = 'set(SPIRV_TOOLS_VERSION "main")'
        pinned = 'set(SPIRV_TOOLS_VERSION "'+RECIPES["pins"]["chipstar_spirv_tools"]+'")'
        if upstream in text:
            text = text.replace(upstream, pinned)
        elif pinned not in text:
            raise RuntimeError("chipStar SPIRV-Tools source pin cannot be applied")
        # A full commit pin must stay fetchable after the upstream branch moves;
        # a depth-one clone only contains the then-current branch tips.
        shallow = "GIT_TAG          ${SPIRV_TOOLS_VERSION}\n    GIT_SHALLOW      TRUE"
        complete = "GIT_TAG          ${SPIRV_TOOLS_VERSION}\n    GIT_SHALLOW      FALSE"
        if shallow in text:
            text = text.replace(shallow, complete)
        elif complete not in text:
            raise RuntimeError("chipStar SPIRV-Tools full-history clone patch cannot be applied")
        cmake.write_text(text)
        major = int(subprocess.check_output([llvm/"bin/llvm-config", "--version"], text=True).split(".")[0])
        flags += [f"-DLLVM_CONFIG_BIN={llvm}/bin/llvm-config", f"-DCHIP_LLVM_USE_INTERGRATED_SPIRV={'ON' if major >= 22 else 'OFF'}",
                  "-DCHIP_BUILD_TESTS=OFF", "-DCHIP_BUILD_SAMPLES=OFF"]
        if major < 22:
            translator = base / "install/spirv-translator/bin/llvm-spirv"
            if not translator.exists():
                provision("spirv-translator", args, base, log)
            flags += [f"-DLLVM_SPIRV={translator}"]
            extra["typed_pointer_adapter"] = typed_spirv_adapter(base, translator, major)
    elif name == "spirv-translator":
        llvm = Path(args.llvm_prefix or base / "install/llvm20-shared")
        flags += [f"-DCMAKE_C_COMPILER={llvm}/bin/clang", f"-DCMAKE_CXX_COMPILER={llvm}/bin/clang++",
                  f"-DLLVM_DIR={llvm}/lib/cmake/llvm", "-DLLVM_SPIRV_INCLUDE_TESTS=OFF",
                  "-DBUILD_SHARED_LIBS=ON", f"-DCMAKE_INSTALL_RPATH={base}/install/spirv-translator/lib;{llvm}/lib"]
    else:
        flags += [f"-DCMAKE_CXX_COMPILER={compiler}", "-DKokkos_ENABLE_SERIAL=ON", "-DKokkos_ENABLE_TESTS=OFF",
                  "-DKokkos_ENABLE_EXAMPLES=OFF", "-DCMAKE_CXX_STANDARD=20"]
        backend = component.split("-", 1)[1]
        if backend == "cuda":
            if args.kokkos_cuda_compiler == "clang":
                flags += [f"-DCMAKE_CXX_COMPILER={shutil.which('clang++') or 'clang++'}",
                          f"-DKokkos_CUDA_DIR={os.getenv('CUDA_PATH', '/usr/local/cuda')}"]
            else:
                flags += [f"-DCMAKE_CXX_COMPILER={src}/bin/nvcc_wrapper"]
            arch = os.getenv("FFMT_GPU_ARCH", "sm_90")
            kokkos_arch = {"sm_70": "VOLTA70", "sm_80": "AMPERE80", "sm_86": "AMPERE86", "sm_89": "ADA89", "sm_90": "HOPPER90"}.get(arch)
            if not kokkos_arch:
                raise ValueError("Unsupported Kokkos CUDA architecture selection: " + arch)
            flags += ["-DKokkos_ENABLE_CUDA=ON", f"-DKokkos_ARCH_{kokkos_arch}=ON"]
        elif backend == "hip":
            flags += [f"-DCMAKE_CXX_COMPILER={shutil.which('hipcc') or 'hipcc'}", "-DKokkos_ENABLE_HIP=ON", "-DKokkos_ARCH_AMD_GFX942=ON"]
        elif backend == "sycl":
            flags += [f"-DCMAKE_CXX_COMPILER={os.getenv('FFMT_CXX_SYCL', 'icpx')}", "-DKokkos_ENABLE_SYCL=ON",
                      "-DKokkos_ARCH_INTEL_PVC=ON"]
            dpl = Path("/soft/compilers/oneapi/2025.0.0.825/oneapi/dpl/latest")
            if dpl.exists():
                flags += [f"-DCMAKE_PREFIX_PATH={dpl}"]
    if name == "chipstar":
        # Upstream configuration imports PyYAML even when its tests are off.
        python = os.getenv("FFMT_PYTHON", shutil.which("python3") or "python3")
        python_prefix = base / "install/python-deps"
        if not (python_prefix / "yaml").exists():
            run([python, "-m", "pip", "install", "--disable-pip-version-check", "--target", python_prefix, "PyYAML==6.0.3"], log)
        os.environ["PYTHONPATH"] = str(python_prefix) + os.pathsep + os.environ.get("PYTHONPATH", "")
        flags += [f"-DPython_EXECUTABLE={python}", f"-DPython3_EXECUTABLE={python}"]
    result = cmake_install(component + ("-" + args.install_suffix if args.install_suffix else ""), src, base, flags, args.jobs, log)
    if name == "spirv-translator":
        major = int(subprocess.check_output([llvm/"bin/llvm-config", "--version"], text=True).split(".")[0])
        extra["typed_pointer_adapter"] = typed_spirv_adapter(base, base / "install/spirv-translator/bin/llvm-spirv", major)
    result.update(extra)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", required=True)
    p.add_argument("--component", action="append", required=True, choices=["intel-llvm", "llvm-shared", "adaptivecpp", "chipstar", "spirv-translator",
                   "kokkos-serial", "kokkos-cuda", "kokkos-hip", "kokkos-sycl"])
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--llvm-prefix")
    p.add_argument("--kokkos-cuda-compiler", choices=["clang", "nvcc"], default="clang")
    p.add_argument("--install-suffix", help="Keep a distinct compiler/architecture installation alongside the primary prefix")
    p.add_argument("--resume-build", help="Resume a named LLVM build directory after recovering a missing prerequisite")
    p.add_argument("--fetch-only", action="store_true", help="Download sources without compilation (login-safe)")
    a = p.parse_args()
    if a.install_suffix and any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_" for c in a.install_suffix):
        p.error("--install-suffix accepts letters, digits, dash and underscore")
    if a.resume_build and ("/" in a.resume_build or a.resume_build in (".", "..")):
        p.error("--resume-build must name a directory directly under software/<profile>/build")
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
