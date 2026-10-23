#pragma once
#if defined(__ADAPTIVECPP__) || defined(__HIPSYCL__) || defined(__ACPP__)
#define FFMT_COMPILER_ACPP 1
#else
#define FFMT_COMPILER_ACPP 0
#endif
#ifndef FFMT_BUFFER_PATH
#define FFMT_BUFFER_PATH 1
#endif
#if defined(__CUDACC__) || defined(__HIPCC__)
#define FMT_HD __host__ __device__
#else
#define FMT_HD
#endif
// Host and accelerator passes each keep their own constexpr lookup data.
// NVCC requires explicit device storage for tables indexed at runtime.
#if (defined(__CUDACC__) && defined(__CUDA_ARCH__)) ||                         \
    (defined(__HIPCC__) && defined(__HIP_DEVICE_COMPILE__))
#define FFMT_DEVICE_TABLE __device__
#else
#define FFMT_DEVICE_TABLE
#endif
#ifndef FFMT_BUFFER_SIZE
#define FFMT_BUFFER_SIZE 128
#endif
#ifndef FFMT_UNICODE
#define FFMT_UNICODE 1
#endif
#if defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP
#if FFMT_COMPILER_ACPP
#include <sycl/sycl.hpp>
#else
#include <sycl/ext/oneapi/experimental/builtins.hpp>
#endif
#endif
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <concepts>
#include <iterator>
#include <limits>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
