"""Compiler target selection shared by CUDA, HIP and OpenMP adapters."""
import os
from pathlib import Path
import subprocess

def cuda_architecture():
    explicit = os.environ.get("FFMT_GPU_ARCH")
    if explicit:
        return explicit
    try:
        output = subprocess.check_output(
            ["nvidia-smi", "--query-gpu=compute_cap", "--format=csv,noheader"], timeout=15)
        return "sm_" + output.decode().splitlines()[0].strip().replace(".", "")
    except (OSError, subprocess.SubprocessError, IndexError):
        return "sm_90"

def hip_architecture():
    explicit = os.environ.get("FFMT_GPU_ARCH")
    if explicit:
        return explicit
    rocm = Path(os.environ.get("ROCM_PATH", "/opt/rocm"))
    try:
        return subprocess.check_output([str(rocm / "llvm/bin/amdgpu-arch")], timeout=15).decode().splitlines()[0]
    except (OSError, subprocess.SubprocessError, IndexError):
        return "gfx942"
