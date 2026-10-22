# representative qualification

Exact byte comparisons against the pinned host fmt oracle. Completed/attempted counts summarize recorded per-case stdout executions; shared-shard failures are retained separately. Logical case IDs remain stable across configurations. Compile failures and unsupported cases are not execution attempts.

A failed positive control means cases were not exercised. Unsupported or compile-failing cases are not counted as executions. Hardware availability and compiler failures are separate from output agreement.

Source identifies the captured library commit; +W means captured working-tree headers, whose hashes are retained in JSON.

| Profile | Actual device | Route | Opt | Source | Completed/attempted | Exact | Mismatch | Unsupported | Other failures | Negative rejection |
|---|---|---|---:|---|---:|---:|---:|---:|---:|---:|
| rome | host x86_64 | host-buffer | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| rome | host x86_64 | host-buffer | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| rome | host x86_64 | host-specifiers | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| rome | host x86_64 | host-specifiers | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| rome | host x86_64 | kokkos-serial | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| rome | host x86_64 | kokkos-serial | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | host aarch64 | host-buffer | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | host aarch64 | host-buffer | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | host aarch64 | host-specifiers | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | host aarch64 | host-specifiers | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | NVIDIA GH200 480GB | cuda | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | NVIDIA GH200 480GB | cuda | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | NVIDIA GH200 480GB | nvcc | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| gh200 | NVIDIA GH200 480GB | nvcc | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| mi300x | AMD Radeon Graphics | hip | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| mi300x | AMD Radeon Graphics | hip | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| mi300x | AMD Radeon Graphics [kokkos space=HIP] | kokkos-hip | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| mi300x | AMD Radeon Graphics [kokkos space=HIP] | kokkos-hip | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| mi300x | AMD Radeon Graphics | acpp | O0 | 21f9f5ef | 53/54 | 52 | 1 | 0 | 1 | 9/9 |
| h100 | NVIDIA A40 | cuda | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 | cuda | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 | nvcc | O0 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 | nvcc | O2 | 21f9f5ef | 54/54 | 54 | 0 | 0 | 0 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 | sycl | O0 | 21f9f5ef | 54/54 | 53 | 1 | 0 | 0 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 | sycl | O2 | 21f9f5ef | 54/54 | 53 | 1 | 0 | 0 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 [kokkos space=SYCL] | kokkos-sycl | O0 | 21f9f5ef | 54/54 | 53 | 1 | 0 | 0 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 [kokkos space=SYCL] | kokkos-sycl | O2 | 21f9f5ef | 54/54 | 53 | 1 | 0 | 0 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 [openmp device=0 initial_device=false] | omp-spirv | O0 | 21f9f5ef | 54/54 | 53 | 1 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 | kokkos-cuda | O0 | 21f9f5ef | 8/8 | 8 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 | kokkos-cuda | O2 | 21f9f5ef | 8/8 | 8 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 [openmp device=0 initial_device=false] | omp-cuda | O0 | 21f9f5ef | 8/8 | 8 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 [openmp device=0 initial_device=false] | omp-cuda | O2 | 21f9f5ef | 8/8 | 8 | 0 | 0 | 0 | 9/9 |
| h100 | NVIDIA A40 | acpp | O0 | 21f9f5ef | 7/8 | 7 | 0 | 0 | 1 | 9/9 |
| h100 | NVIDIA A40 | acpp | O2 | 21f9f5ef | 7/8 | 7 | 0 | 0 | 1 | 9/9 |
| mi300x | AMD Radeon Graphics [openmp device=0 initial_device=false] | omp-hip | O0 | 98dba67c | 8/8 | 8 | 0 | 0 | 0 | 9/9 |
| mi300x | AMD Radeon Graphics [openmp device=0 initial_device=false] | omp-hip | O2 | 98dba67c | 6/6 | 6 | 0 | 0 | 2 | 9/9 |
| mi300x | AMD Radeon Graphics | acpp | O0 | 98dba67c | 7/8 | 7 | 0 | 0 | 1 | 9/9 |
| mi300x | AMD Radeon Graphics | acpp | O2 | 98dba67c | 7/8 | 7 | 0 | 0 | 1 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 [openmp device=0 initial_device=false] | omp-spirv | O2 | 98dba67c | 4/6 | 4 | 0 | 0 | 4 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 | chipstar | O0 | 98dba67c | 8/8 | 8 | 0 | 0 | 0 | 9/9 |
| chiatta | Intel(R) Data Center GPU Max 1550 | chipstar | O2 | 98dba67c | 8/8 | 8 | 0 | 0 | 0 | 9/9 |

## Observed gaps

- **mi300x partial campaign:** only the selected terminal configurations are shown; the original run did not finish. Unfinished attempts and termination scope remain in the JSON.
- **mi300x / acpp / O0 / 21f9f5ef, mismatch:** nul-char.
- **mi300x / acpp / O0 / 21f9f5ef, execution_timeout:** chrono-calendar.
- **h100 partial campaign:** only the selected terminal configurations are shown; the original run did not finish. Unfinished attempts and termination scope remain in the JSON.
- **chiatta partial campaign:** only the selected terminal configurations are shown; the original run did not finish. Unfinished attempts and termination scope remain in the JSON.
- **chiatta / sycl / O0 / 21f9f5ef, mismatch:** nul-char.
- **chiatta / sycl / O2 / 21f9f5ef, mismatch:** nul-char.
- **chiatta / kokkos-sycl / O0 / 21f9f5ef, mismatch:** nul-char.
- **chiatta / kokkos-sycl / O2 / 21f9f5ef, mismatch:** nul-char.
- **chiatta / omp-spirv / O0 / 21f9f5ef, mismatch:** nul-char.
- **h100 / acpp / O0 / 21f9f5ef, execution_timeout:** chrono-calendar.
- **h100 / acpp / O2 / 21f9f5ef, execution_timeout:** chrono-calendar.
- **mi300x / omp-hip / O2 / 98dba67c, compile_failure:** chrono-calendar, bounded-family-padding.
- **mi300x / acpp / O0 / 98dba67c, execution_timeout:** chrono-calendar.
- **mi300x / acpp / O2 / 98dba67c, execution_timeout:** chrono-calendar.
- **chiatta / omp-spirv / O2 / 98dba67c, compile_failure:** chrono-duration, chrono-calendar.
- **chiatta / omp-spirv / O2 / 98dba67c, execution_failure:** bounded-family-padding.
- **chiatta / omp-spirv / O2 / 98dba67c, execution_timeout:** stream-left-family-padding.

## Device-memory numeric comparisons

Each input record combines five independently referenced formats. Counts are input records, not stdout cases.

| Profile | Actual device | Route | Opt | Source | Status | Exact/input records |
|---|---|---|---:|---|---|---:|
| rome | host x86_64 | host-buffer | O0 | 21f9f5ef | pass | 8216/8216 |
| rome | host x86_64 | host-buffer | O2 | 21f9f5ef | pass | 8216/8216 |
| rome | host x86_64 | host-specifiers | O0 | 21f9f5ef | pass | 8216/8216 |
| rome | host x86_64 | host-specifiers | O2 | 21f9f5ef | pass | 8216/8216 |
| rome | host x86_64 | kokkos-serial | O0 | 21f9f5ef | pass | 8216/8216 |
| rome | host x86_64 | kokkos-serial | O2 | 21f9f5ef | pass | 8216/8216 |
| gh200 | host aarch64 | host-buffer | O0 | 21f9f5ef | pass | 8216/8216 |
| gh200 | host aarch64 | host-buffer | O2 | 21f9f5ef | pass | 8216/8216 |
| gh200 | host aarch64 | host-specifiers | O0 | 21f9f5ef | pass | 8216/8216 |
| gh200 | host aarch64 | host-specifiers | O2 | 21f9f5ef | pass | 8216/8216 |
| gh200 | NVIDIA GH200 480GB | cuda | O0 | 21f9f5ef | pass | 8216/8216 |
| gh200 | NVIDIA GH200 480GB | cuda | O2 | 21f9f5ef | pass | 8216/8216 |
| gh200 | NVIDIA GH200 480GB | nvcc | O0 | 21f9f5ef | pass | 8216/8216 |
| gh200 | NVIDIA GH200 480GB | nvcc | O2 | 21f9f5ef | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics | hip | O0 | 21f9f5ef | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics | hip | O2 | 21f9f5ef | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics [kokkos space=HIP] | kokkos-hip | O0 | 21f9f5ef | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics [kokkos space=HIP] | kokkos-hip | O2 | 21f9f5ef | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics | acpp | O0 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | cuda | O0 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | cuda | O2 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | nvcc | O0 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | nvcc | O2 | 21f9f5ef | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 | sycl | O0 | 21f9f5ef | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 | sycl | O2 | 21f9f5ef | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 [kokkos space=SYCL] | kokkos-sycl | O0 | 21f9f5ef | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 [kokkos space=SYCL] | kokkos-sycl | O2 | 21f9f5ef | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 [openmp device=0 initial_device=false] | omp-spirv | O0 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | kokkos-cuda | O0 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | kokkos-cuda | O2 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 [openmp device=0 initial_device=false] | omp-cuda | O0 | 21f9f5ef | execution_failure | —/— |
| h100 | NVIDIA A40 [openmp device=0 initial_device=false] | omp-cuda | O2 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | acpp | O0 | 21f9f5ef | pass | 8216/8216 |
| h100 | NVIDIA A40 | acpp | O2 | 21f9f5ef | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics [openmp device=0 initial_device=false] | omp-hip | O0 | 98dba67c | execution_failure | —/— |
| mi300x | AMD Radeon Graphics [openmp device=0 initial_device=false] | omp-hip | O2 | 98dba67c | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics | acpp | O0 | 98dba67c | pass | 8216/8216 |
| mi300x | AMD Radeon Graphics | acpp | O2 | 98dba67c | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 [openmp device=0 initial_device=false] | omp-spirv | O2 | 98dba67c | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 | chipstar | O0 | 98dba67c | pass | 8216/8216 |
| chiatta | Intel(R) Data Center GPU Max 1550 | chipstar | O2 | 98dba67c | pass | 8216/8216 |

The companion JSON contains compiler/device identity, source file hashes, commands, per-case outcomes, and hashes/paths for raw evidence retained under artifacts/. Code coverage and performance are measured separately.
