// Select one launcher for shared cases. The pinned fmt oracle always runs on
// the host; only the ffmt half runs in a SYCL queue, OpenMP target region, or
// native CUDA/HIP kernel. Check compiler predefines before base.hpp is included.
#if defined(SYCL_LANGUAGE_VERSION) \
    || defined(__ADAPTIVECPP__) || defined(__HIPSYCL__) || defined(__ACPP__)
#include "test_body.inc"
#elif defined(_OPENMP)
#include "test_body_omp.inc"
#elif defined(__CUDACC__) || defined(__HIPCC__)
#include "test_body_cuda.inc"
#else
#include "test_body_host.inc"
#endif
