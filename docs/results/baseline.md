# baseline qualification

Exact byte comparisons against the pinned host fmt oracle. Completed/attempted counts summarize recorded per-case stdout executions; shared-shard failures are retained separately. Logical case IDs remain stable across configurations. Compile failures and unsupported cases are not execution attempts.

A failed positive control means cases were not exercised. Unsupported or compile-failing cases are not counted as executions. Hardware availability and compiler failures are separate from output agreement.

Source identifies the captured library commit; +W means captured working-tree headers, whose hashes are retained in JSON.

| Profile | Actual device | Route | Opt | Source | Completed/attempted | Exact | Mismatch | Unsupported | Other failures | Negative rejection |
|---|---|---|---:|---|---:|---:|---:|---:|---:|---:|
| chiatta | unavailable | sycl | O0 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| chiatta | unavailable | sycl | O2 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| chiatta | unavailable | omp-spirv | O0 | 3fe258a8 | 0/40 | 0 | 0 | 0 | 40 | 0/0 |
| chiatta | unavailable | omp-spirv | O2 | 3fe258a8 | 0/40 | 0 | 0 | 0 | 40 | 0/0 |
| gh200 | host aarch64 | host-buffer | O0 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| gh200 | host aarch64 | host-buffer | O2 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| gh200 | host aarch64 | host-specifiers | O0 | 3fe258a8 | 30/30 | 24 | 6 | 10 | 0 | 9/9 |
| gh200 | host aarch64 | host-specifiers | O2 | 3fe258a8 | 30/30 | 24 | 6 | 10 | 0 | 9/9 |
| gh200 | unavailable | cuda | O0 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| gh200 | unavailable | cuda | O2 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| gh200 | unavailable | nvcc | O0 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| gh200 | unavailable | nvcc | O2 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| h100 | NVIDIA H100 80GB HBM3 | cuda | O0 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| h100 | NVIDIA H100 80GB HBM3 | cuda | O2 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| h100 | unavailable | nvcc | O0 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| h100 | unavailable | nvcc | O2 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| h100 | openmp device=0 initial_device=false | omp-cuda | O0 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| h100 | openmp device=0 initial_device=false | omp-cuda | O2 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| mi300x | unavailable | hip | O0 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | unavailable | hip | O2 | 3fe258a8 | 0/0 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | openmp device=0 initial_device=false | omp-hip | O0 | 3fe258a8 | 0/40 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | openmp device=0 initial_device=false | omp-hip | O2 | 3fe258a8 | 0/40 | 0 | 0 | 0 | 40 | 0/0 |
| rome | host x86_64 | host-buffer | O0 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| rome | host x86_64 | host-buffer | O2 | 3fe258a8 | 36/36 | 31 | 5 | 4 | 0 | 9/9 |
| rome | host x86_64 | host-specifiers | O0 | 3fe258a8 | 30/30 | 24 | 6 | 10 | 0 | 9/9 |
| rome | host x86_64 | host-specifiers | O2 | 3fe258a8 | 30/30 | 24 | 6 | 10 | 0 | 9/9 |

## Observed gaps

- **chiatta / sycl / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **chiatta / sycl / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **chiatta / omp-spirv / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **chiatta / omp-spirv / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **gh200 / host-buffer / O0 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **gh200 / host-buffer / O0 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **gh200 / host-buffer / O2 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **gh200 / host-buffer / O2 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **gh200 / host-specifiers / O0 / 3fe258a8, unsupported:** signed-hex, binary-max, zero-alt, center-fill, dynamic-width, dynamic-precision, float-hex, unicode-fill, debug-string, debug-char.
- **gh200 / host-specifiers / O0 / 3fe258a8, mismatch:** float-shortest, float-notation, float32-subnormal, float64-subnormal, unicode-width, null-pointer.
- **gh200 / host-specifiers / O2 / 3fe258a8, unsupported:** signed-hex, binary-max, zero-alt, center-fill, dynamic-width, dynamic-precision, float-hex, unicode-fill, debug-string, debug-char.
- **gh200 / host-specifiers / O2 / 3fe258a8, mismatch:** float-shortest, float-notation, float32-subnormal, float64-subnormal, unicode-width, null-pointer.
- **gh200 / cuda / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **gh200 / cuda / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **gh200 / nvcc / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **gh200 / nvcc / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **h100 / cuda / O0 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **h100 / cuda / O0 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **h100 / cuda / O2 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **h100 / cuda / O2 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **h100 / nvcc / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **h100 / nvcc / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **h100 / omp-cuda / O0 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **h100 / omp-cuda / O0 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **h100 / omp-cuda / O2 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **h100 / omp-cuda / O2 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **mi300x / hip / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / hip / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / omp-hip / O0 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / omp-hip / O2 / 3fe258a8:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **rome / host-buffer / O0 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **rome / host-buffer / O0 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **rome / host-buffer / O2 / 3fe258a8, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **rome / host-buffer / O2 / 3fe258a8, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **rome / host-specifiers / O0 / 3fe258a8, unsupported:** signed-hex, binary-max, zero-alt, center-fill, dynamic-width, dynamic-precision, float-hex, unicode-fill, debug-string, debug-char.
- **rome / host-specifiers / O0 / 3fe258a8, mismatch:** float-shortest, float-notation, float32-subnormal, float64-subnormal, unicode-width, null-pointer.
- **rome / host-specifiers / O2 / 3fe258a8, unsupported:** signed-hex, binary-max, zero-alt, center-fill, dynamic-width, dynamic-precision, float-hex, unicode-fill, debug-string, debug-char.
- **rome / host-specifiers / O2 / 3fe258a8, mismatch:** float-shortest, float-notation, float32-subnormal, float64-subnormal, unicode-width, null-pointer.

## Device-memory numeric comparisons

Each input record combines five independently referenced formats. Counts are input records, not stdout cases.

| Profile | Actual device | Route | Opt | Source | Status | Exact/input records |
|---|---|---|---:|---|---|---:|

The companion JSON contains compiler/device identity, source file hashes, commands, per-case outcomes, and hashes/paths for raw evidence retained under artifacts/. Code coverage and performance are measured separately.
