#!/bin/bash
# Source this only inside an allocation. Local toolchains override shared ones.
ffmt_system=${1:?profile required}
ffmt_user_sycl=${FFMT_CXX_SYCL:-}
ffmt_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
if [ -f /etc/profile.d/modules.sh ]; then source /etc/profile.d/modules.sh; fi
if type module >/dev/null 2>&1; then
  module use /soft/modulefiles
  if [ "$(uname -m)" = x86_64 ]; then
    module load gcc/13.3.0 || true
    module load llvm/release-22.1.8 || true
    module load cmake/3.28.3 || true
    case "$ffmt_system" in
      h100) module load cuda/12.9.1 || true ;;
      mi300x)
        module load rocm/7.0.2 || true
        # LLVM22+ROCm7 drops output on MI250 and faults during printf on
        # MI300X. A matched ROCm20 compiler/runtime passes isolated probes.
        export FFMT_CXX_OMP_HIP="${FFMT_CXX_OMP_HIP:-$ROCM_PATH/llvm/bin/clang++}"
        export FFMT_OMP_RUNTIME_PREFIX="${FFMT_OMP_RUNTIME_PREFIX:-$ROCM_PATH/llvm}"
        ;;
      chiatta)
        module load intel_compute_runtime/release/821.36 || true
        # The 821 runtime initializes Level Zero but exposes no UR devices to
        # Intel LLVM 7.2. The measured 1032 stack enumerates Max 1550 correctly.
        export LD_LIBRARY_PATH="/soft/libraries/intel-gpu-umd/1032.21/lib64:/soft/libraries/intel-gpu-umd/1032.21/lib64/intel-opencl:${LD_LIBRARY_PATH:-}"
        ffmt_oneapi=/soft/compilers/oneapi/2025.0.0.825/oneapi/compiler/latest
        if [ -d "$ffmt_oneapi/bin" ]; then
          export PATH="$ffmt_oneapi/bin:$PATH"
          export LD_LIBRARY_PATH="$ffmt_oneapi/lib:$ffmt_oneapi/opt/compiler/lib:${LD_LIBRARY_PATH:-}"
          export FFMT_CXX_SYCL="${ffmt_user_sycl:-$ffmt_oneapi/bin/icpx}"
          export FFMT_CXX_OMP_SPIRV="${FFMT_CXX_OMP_SPIRV:-$ffmt_oneapi/bin/icpx}"
        fi
        export ONEAPI_DEVICE_SELECTOR=level_zero:gpu
        export CHIP_BE="${CHIP_BE:-level0}" CHIP_DEVICE_TYPE="${CHIP_DEVICE_TYPE:-gpu}"
        unset ffmt_oneapi
        ;;
    esac
  elif [ "$(uname -m)" = aarch64 ]; then
    export FFMT_PYTHON=/usr/bin/python3.12
    # Keep native defaults while honoring explicit general, oracle and route
    # compiler selections. Python remains native even with an inherited x86 PATH.
    export CXX="${CXX:-/usr/bin/g++-14}"
    export FFMT_HOST_CXX="${FFMT_HOST_CXX:-$CXX}"
    export FFMT_CXX_HOST_BUFFER="${FFMT_CXX_HOST_BUFFER:-$CXX}"
    export FFMT_CXX_HOST_SPECIFIERS="${FFMT_CXX_HOST_SPECIFIERS:-$CXX}"
    export FFMT_FLAGS_NVCC="-ccbin=/usr/bin/g++-14 ${FFMT_FLAGS_NVCC:-}"
    export FFMT_FLAGS_CUDA="--gcc-install-dir=/usr/lib64/gcc/aarch64-suse-linux/14 ${FFMT_FLAGS_CUDA:-}"
    # /soft is architecture-specific on JLSE, but module names vary by node.
    for ffmt_version in release-23.1.3 release-22.1.8 release-21.1.0 release-20.1.8 release-19.1.0; do
      if [ -x "/soft/compilers/llvm/$ffmt_version/bin/clang++" ]; then
        export PATH="/soft/compilers/llvm/$ffmt_version/bin:$PATH"
        break
      fi
    done
    for ffmt_version in cuda-12.9.1 cuda-12.3.0 cuda-12.2.2 cuda-12.0.0; do
      if [ -x "/soft/compilers/cuda/$ffmt_version/bin/nvcc" ]; then
        export CUDA_PATH="/soft/compilers/cuda/$ffmt_version"
        export PATH="$CUDA_PATH/bin:$PATH"
        export LD_LIBRARY_PATH="$CUDA_PATH/lib64:${LD_LIBRARY_PATH:-}"
        break
      fi
    done
    unset ffmt_version
  fi
fi
ffmt_prefix="$ffmt_root/software/$ffmt_system/install"
for ffmt_tool in gcc llvm cuda intel-llvm adaptivecpp spirv-translator chipstar kokkos; do
  if [ -d "$ffmt_prefix/$ffmt_tool/bin" ]; then export PATH="$ffmt_prefix/$ffmt_tool/bin:$PATH"; fi
  if [ -d "$ffmt_prefix/$ffmt_tool/lib" ]; then export LD_LIBRARY_PATH="$ffmt_prefix/$ffmt_tool/lib:${LD_LIBRARY_PATH:-}"; fi
  if [ -d "$ffmt_prefix/$ffmt_tool/lib64" ]; then export LD_LIBRARY_PATH="$ffmt_prefix/$ffmt_tool/lib64:${LD_LIBRARY_PATH:-}"; fi
done
# The shared GCC 13 build omits a GLIBCXX_3.4.21 futex symbol used by newer
# CMake/LLVM binaries. Isolate the native OS libstdc++ rather than shadowing
# accelerator libraries with the whole system library directory.
if [ -f /usr/lib64/libstdc++.so.6 ]; then
  mkdir -p "$ffmt_prefix/host-runtime/lib"
  ln -sfn /usr/lib64/libstdc++.so.6 "$ffmt_prefix/host-runtime/lib/libstdc++.so.6"
  export LD_LIBRARY_PATH="$ffmt_prefix/host-runtime/lib:${LD_LIBRARY_PATH:-}"
fi
if [ -x "$ffmt_prefix/intel-llvm/bin/clang++" ]; then export FFMT_CXX_SYCL="${ffmt_user_sycl:-$ffmt_prefix/intel-llvm/bin/clang++}"; fi
if [ -x "$ffmt_prefix/adaptivecpp/bin/acpp" ]; then export FFMT_CXX_ACPP="${FFMT_CXX_ACPP:-$ffmt_prefix/adaptivecpp/bin/acpp}"; fi
if [ -x "$ffmt_prefix/chipstar/bin/hipcc" ]; then
  export FFMT_CXX_CHIPSTAR="${FFMT_CXX_CHIPSTAR:-$ffmt_prefix/chipstar/bin/hipcc}"
  # The locally built Clang otherwise selects native GCC14 headers, whose
  # hardened string_view indexing calls a host-only assertion on SPIR devices.
  if [ "$(uname -m)" = x86_64 ] && [ -z "${FFMT_FLAGS_CHIPSTAR:-}" ]; then
    export FFMT_FLAGS_CHIPSTAR=--gcc-toolchain=/soft/compilers/gcc/13.3.0/x86_64-suse-linux
    if [ -d "$ffmt_prefix/spirv-translator-typed/bin" ]; then
      export FFMT_FLAGS_CHIPSTAR="$FFMT_FLAGS_CHIPSTAR -B$ffmt_prefix/spirv-translator-typed/bin"
    fi
  fi
fi
if [ -n "${FFMT_OMP_RUNTIME_PREFIX:-}" ]; then
  export LD_LIBRARY_PATH="$FFMT_OMP_RUNTIME_PREFIX/lib:$FFMT_OMP_RUNTIME_PREFIX/lib/x86_64-unknown-linux-gnu:${LD_LIBRARY_PATH:-}"
fi
for ffmt_backend in serial cuda hip sycl; do
  ffmt_export="FFMT_KOKKOS_${ffmt_backend^^}_PREFIX"
  if [ -d "$ffmt_prefix/kokkos-$ffmt_backend" ] && [ -z "${!ffmt_export:-}" ]; then
    printf -v "$ffmt_export" '%s' "$ffmt_prefix/kokkos-$ffmt_backend"
    export "$ffmt_export"
  fi
done
unset ffmt_backend ffmt_export ffmt_user_sycl
export OMP_TARGET_OFFLOAD=MANDATORY LC_ALL=C GOMAXPROCS=2
unset ffmt_tool ffmt_prefix ffmt_system ffmt_root
