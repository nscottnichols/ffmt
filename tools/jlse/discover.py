#!/usr/bin/env python3
"""Capture non-secret compiler and hardware evidence inside a Cobalt job."""
import argparse
import datetime
import glob
import json
import os
from pathlib import Path
import platform
import shutil
import shlex
import socket
import subprocess


def command(argv, timeout=45):
    try:
        p = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=timeout, universal_newlines=True, errors="replace")
        return {"command": argv, "returncode": p.returncode,
                "stdout": p.stdout, "stderr": p.stderr}
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"command": argv, "error": str(exc)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    evidence = {"schema_version": 1, "profile": args.profile,
                "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "hostname": socket.gethostname(), "machine": platform.machine(),
                "platform": platform.platform(), "python_version": platform.python_version(),
                "cobalt_jobid": os.getenv("COBALT_JOBID"),
                "cobalt_partname": os.getenv("COBALT_PARTNAME"), "commands": {},
                "paths": {}, "environment": {k: os.getenv(k) for k in
                    ["PATH", "LD_LIBRARY_PATH", "LIBRARY_PATH", "CPATH", "LOADEDMODULES",
                     "ONEAPI_DEVICE_SELECTOR", "CHIP_BE", "CHIP_DEVICE_TYPE", "CHIP_SKIP_UNINIT", "FFMT_FLAGS_CHIPSTAR"]}}
    candidates = ["g++", "gcc", "clang++", "clang", "nvcc", "hipcc", "icpx", "acpp", "syclcc"]
    for name in candidates:
        executable = shutil.which(name)
        if executable:
            evidence["commands"][name] = command([executable, "--version"])
    evidence["selected_compilers"] = {}
    for key in sorted(k for k in os.environ if k == "CXX" or k == "FFMT_HOST_CXX" or k.startswith("FFMT_CXX_")):
        value = os.environ[key]
        evidence["selected_compilers"][key] = command([*shlex.split(value), "--version"])
    for name, argv in {
        "lscpu": ["lscpu"], "lspci": ["lspci", "-nn"],
        "nvidia-smi": ["nvidia-smi"],
        "nvidia-query": ["nvidia-smi", "--query-gpu=name,driver_version,compute_cap,pci.bus_id", "--format=csv"],
        "rocminfo": ["rocminfo"], "rocm_agent_enumerator": ["rocm_agent_enumerator"],
        "sycl-ls": ["sycl-ls"], "clinfo": ["clinfo"],
    }.items():
        if shutil.which(argv[0]):
            evidence["commands"][name] = command(argv)
    # Shallow explicit globs avoid crawling shared installations or user homes.
    for pattern in ["/soft/*", "/opt/*", "/usr/local/*", "/soft/modulefiles/*",
                    "/soft/compilers/*", "/soft/compilers/llvm/*", "/soft/compilers/cuda/*",
                    "/soft/modulefiles/llvm/*", "/soft/modulefiles/cuda/*", "/soft/modulefiles/gcc/*",
                    "/opt/nvidia/*", "/soft/nvidia/*", "/soft/nvidia/*/*",
                    "/soft/intel/*", "/opt/intel/oneapi/*", "/soft/llvm/*",
                    "/soft/rocm/*", "/usr/bin/g++*", "/usr/bin/clang++*",
                    "/usr/local/cuda*/bin/nvcc", "/opt/nvidia/hpc_sdk/*/*/compilers/bin/*++",
                    "/usr/bin/python3*", "/usr/bin/gcc*", "/opt/nvidia/hpc_sdk/*/*/cuda/*/bin/nvcc"]:
        evidence["paths"][pattern] = sorted(glob.glob(pattern))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps({k: evidence[k] for k in ["profile", "hostname", "machine", "cobalt_jobid"]}))
    print(f"Discovery evidence: {args.output}")


if __name__ == "__main__":
    main()
