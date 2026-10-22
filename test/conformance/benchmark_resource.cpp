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
#ifndef BENCH_FLOAT
#define BENCH_FLOAT 0
#endif
#ifndef BENCH_PRINT
#define BENCH_PRINT 0
#endif
#ifndef BENCH_CALLS
#define BENCH_CALLS 1
#endif
BENCH_HD inline void resource_call(char* output, int value) {
  benchmark_call(output,value,BENCH_FLOAT,BENCH_PRINT,0);
#if BENCH_CALLS == 2
  benchmark_call(output+64,value+1,BENCH_FLOAT,BENCH_PRINT,0);
#endif
}
#if defined(BENCH_CUDA) || defined(BENCH_HIP)
__global__ void ffmt_resource_kernel(char* output,int value) {resource_call(output,value);}
#elif defined(BENCH_SYCL)
int main(int argc,char**) {
  sycl::queue q{sycl::gpu_selector_v};
  char* output=sycl::malloc_shared<char>(128,q);
  q.single_task([=]{resource_call(output,argc);}).wait_and_throw();
  sycl::free(output,q);
}
#elif defined(BENCH_OMP)
int main(int argc,char**) {
  char output[128]{};
#pragma omp target map(from:output[0:128]) firstprivate(argc)
  {resource_call(output,argc);}
}
#else
extern "C" void ffmt_resource_entry(char* output,int value) {resource_call(output,value);}
#endif
