// Wall time includes launch and synchronization for accelerator routes. It is
// intentionally reported separately from host formatting and stdout transport.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#if defined(BENCH_CUDA)
#include <cuda_runtime.h>
#elif defined(BENCH_HIP)
#include <hip/hip_runtime.h>
#elif defined(BENCH_SYCL)
#include <sycl/sycl.hpp>
#elif defined(BENCH_OMP)
#include <omp.h>
#endif
#include "benchmark_calls.hpp"

BENCH_HD void benchmark_batch(char *output, std::size_t *sizes, int iterations,
                              int floating, int printing, int native_printf,
                              int calls) {
  for (int i = 0; i < iterations; ++i) {
    const int value = i % 255 - 127;
    for (int c = 0; c < calls; ++c) {
      const int slot = i * calls + c;
      char *destination = output + slot * 64;
#if defined(BENCH_MULTI_TU)
      if (c)
        sizes[slot] = benchmark_extra(destination, value, floating, printing,
                                      native_printf);
      else
#endif
        sizes[slot] = benchmark_call(destination, value, floating, printing,
                                     native_printf);
    }
  }
}
#if defined(BENCH_CUDA) || defined(BENCH_HIP)
__global__ void benchmark_kernel(char *output, std::size_t *sizes,
                                 int iterations, int floating, int printing,
                                 int native_printf, int calls) {
  benchmark_batch(output, sizes, iterations, floating, printing, native_printf,
                  calls);
}
#elif defined(BENCH_SYCL)
class benchmark_sycl_kernel;
#endif

int main(int argc, char **argv) {
  if (argc != 7)
    return 2;
  const int floating = std::atoi(argv[1]);
  const int printing = std::atoi(argv[2]);
  const int native_printf = std::atoi(argv[3]);
  const int calls = std::atoi(argv[4]);
  const int iterations = std::atoi(argv[5]);
  const bool verify = std::atoi(argv[6]);
  if (iterations <= 0 || iterations > 1000000 || (calls != 1 && calls != 2))
    return 2;
  const std::size_t slots = static_cast<std::size_t>(iterations) * calls;
  const std::size_t bytes = slots * 64;
  std::vector<char> storage(bytes, 0);
  std::vector<std::size_t> lengths(slots, 0);
  char *output = storage.data();
  std::size_t *sizes = lengths.data();
  const char *timer = "host_wall";
  const char *device = "host";
  double jit_prepare_ns = 0;
#if defined(BENCH_CUDA)
  cudaDeviceProp props{};
  if (cudaGetDeviceProperties(&props, 0) != cudaSuccess)
    return 77;
  device = props.name;
  timer = "kernel_launch_and_sync_wall";
  if (cudaMalloc(&output, bytes) != cudaSuccess ||
      cudaMalloc(&sizes, slots * sizeof(std::size_t)) != cudaSuccess)
    return 78;
  if (cudaDeviceSetLimit(cudaLimitPrintfFifoSize, 8 * 1024 * 1024) !=
      cudaSuccess)
    return 78;
#elif defined(BENCH_HIP)
  hipDeviceProp_t props{};
  if (hipGetDeviceProperties(&props, 0) != hipSuccess)
    return 77;
  device = props.name;
  timer = "kernel_launch_and_sync_wall";
  if (hipMalloc(&output, bytes) != hipSuccess ||
      hipMalloc(&sizes, slots * sizeof(std::size_t)) != hipSuccess)
    return 78;
#elif defined(BENCH_SYCL)
  sycl::queue queue{sycl::gpu_selector_v,
                    sycl::property::queue::enable_profiling{}};
  auto name = queue.get_device().get_info<sycl::info::device::name>();
  device = name.c_str();
  timer = "kernel_launch_and_sync_wall";
  output = sycl::malloc_shared<char>(bytes, queue);
  sizes = sycl::malloc_shared<std::size_t>(slots, queue);
  if (!output || !sizes)
    return 78;
#if !FFMT_COMPILER_ACPP
  const auto jit_start = std::chrono::steady_clock::now();
  const auto executable_bundle =
      sycl::get_kernel_bundle<sycl::bundle_state::executable>(
          queue.get_context(), {queue.get_device()},
          {sycl::get_kernel_id<benchmark_sycl_kernel>()});
  jit_prepare_ns = std::chrono::duration<double, std::nano>(
                       std::chrono::steady_clock::now() - jit_start)
                       .count();
#endif
#elif defined(BENCH_OMP)
  int initial = 1;
#pragma omp target map(from : initial)
  {
    initial = omp_is_initial_device();
  }
  if (initial)
    return 77;
  timer = "kernel_launch_and_sync_wall";
  device = "OpenMP target (mandatory offload)";
  output =
      static_cast<char *>(omp_target_alloc(bytes, omp_get_default_device()));
  sizes = static_cast<std::size_t *>(
      omp_target_alloc(slots * sizeof(std::size_t), omp_get_default_device()));
  if (!output || !sizes)
    return 78;
#endif
  // Verification executes exactly one batch. Measurement separates first use,
  // five warm-ups and thirty samples in the same initialized process.
  const int batches = verify ? 1 : 36;
  double durations[36]{};
  double kernel_durations[36]{};
  std::uint64_t checksum = 1469598103934665603ull;
  for (int batch = 0; batch < batches; ++batch) {
    const auto start = std::chrono::steady_clock::now();
#if defined(BENCH_CUDA)
    benchmark_kernel<<<1, 1>>>(output, sizes, iterations, floating, printing,
                               native_printf, calls);
    if (cudaGetLastError() != cudaSuccess ||
        cudaDeviceSynchronize() != cudaSuccess)
      return 79;
#elif defined(BENCH_HIP)
    hipLaunchKernelGGL(benchmark_kernel, dim3(1), dim3(1), 0, 0, output, sizes,
                       iterations, floating, printing, native_printf, calls);
    if (hipGetLastError() != hipSuccess || hipDeviceSynchronize() != hipSuccess)
      return 79;
#elif defined(BENCH_SYCL)
    auto event = queue.submit([&](sycl::handler &handler) {
#if !FFMT_COMPILER_ACPP
      handler.use_kernel_bundle(executable_bundle);
#endif
      handler.single_task<benchmark_sycl_kernel>([=] {
        benchmark_batch(output, sizes, iterations, floating, printing,
                        native_printf, calls);
      });
    });
    event.wait_and_throw();
#elif defined(BENCH_OMP)
#pragma omp target is_device_ptr(output, sizes)                                \
    firstprivate(iterations, floating, printing, native_printf, calls)
    {
      benchmark_batch(output, sizes, iterations, floating, printing,
                      native_printf, calls);
    }
#else
    benchmark_batch(output, sizes, iterations, floating, printing,
                    native_printf, calls);
#endif
    if (printing && std::fflush(stdout))
      return 80;
    const auto finish = std::chrono::steady_clock::now();
    durations[batch] =
        std::chrono::duration<double, std::nano>(finish - start).count();
#if defined(BENCH_SYCL)
    kernel_durations[batch] = static_cast<double>(
        event.get_profiling_info<sycl::info::event_profiling::command_end>() -
        event.get_profiling_info<sycl::info::event_profiling::command_start>());
#endif
    if (!printing) {
#if defined(BENCH_CUDA)
      if (cudaMemcpy(storage.data(), output, bytes, cudaMemcpyDeviceToHost) !=
              cudaSuccess ||
          cudaMemcpy(lengths.data(), sizes, slots * sizeof(std::size_t),
                     cudaMemcpyDeviceToHost) != cudaSuccess)
        return 79;
#elif defined(BENCH_HIP)
      if (hipMemcpy(storage.data(), output, bytes, hipMemcpyDeviceToHost) !=
              hipSuccess ||
          hipMemcpy(lengths.data(), sizes, slots * sizeof(std::size_t),
                    hipMemcpyDeviceToHost) != hipSuccess)
        return 79;
#elif defined(BENCH_SYCL)
      for (std::size_t s = 0; s < slots; ++s)
        lengths[s] = sizes[s];
      for (std::size_t s = 0; s < bytes; ++s)
        storage[s] = output[s];
#elif defined(BENCH_OMP)
      if (omp_target_memcpy(storage.data(), output, bytes, 0, 0,
                            omp_get_initial_device(),
                            omp_get_default_device()) ||
          omp_target_memcpy(lengths.data(), sizes, slots * sizeof(std::size_t),
                            0, 0, omp_get_initial_device(),
                            omp_get_default_device()))
        return 79;
#endif
      for (std::size_t slot = 0; slot < slots; ++slot) {
        if (lengths[slot] > 64)
          return 81;
        for (std::size_t j = 0; j < lengths[slot]; ++j) {
          checksum ^= static_cast<unsigned char>(storage[slot * 64 + j]);
          checksum *= 1099511628211ull;
        }
        if (verify && std::fwrite(storage.data() + slot * 64, 1, lengths[slot],
                                  stdout) != lengths[slot])
          return 80;
      }
    }
  }
  // Device names are emitted separately to keep the numeric record plain JSON.
  std::fprintf(stderr, "FFMT_BENCH_DEVICE %s\n", device);
  std::fprintf(stderr,
               "{\"schema_version\":1,\"timer\":\"%s\",\"first_batch_ns\":%.0f,"
               "\"jit_prepare_ns\":%.0f,\"warmups\":5,\"samples_ns\":[",
               timer, durations[0], jit_prepare_ns);
  if (!verify)
    for (int i = 6; i < 36; ++i)
      std::fprintf(stderr, "%s%.0f", i == 6 ? "" : ",", durations[i]);
  std::fprintf(stderr, "],\"device_kernel_samples_ns\":[");
#if defined(BENCH_SYCL)
  if (!verify)
    for (int i = 6; i < 36; ++i)
      std::fprintf(stderr, "%s%.0f", i == 6 ? "" : ",", kernel_durations[i]);
#endif
  std::fprintf(stderr,
               "],\"iterations\":%d,\"calls\":%d,\"checksum\":\"%llu\"}\n",
               iterations, calls, static_cast<unsigned long long>(checksum));
#if defined(BENCH_CUDA)
  cudaFree(output);
  cudaFree(sizes);
#elif defined(BENCH_HIP)
  hipFree(output);
  hipFree(sizes);
#elif defined(BENCH_SYCL)
  sycl::free(output, queue);
  sycl::free(sizes, queue);
#elif defined(BENCH_OMP)
  omp_target_free(output, omp_get_default_device());
  omp_target_free(sizes, omp_get_default_device());
#endif
  return 0;
}
