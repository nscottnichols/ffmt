# baseline qualification

Exact byte comparisons against the pinned host fmt oracle. Counts are executions per configuration; logical case IDs remain stable across configurations.

A failed positive control means cases were not exercised. Unsupported or compile-failing cases are not counted as executions. Hardware availability and compiler failures are separate from output agreement.

| Profile | Route | Opt | Executed | Exact | Mismatch | Unsupported | Other failures | Negative rejection |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| chiatta | sycl | O0 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| chiatta | sycl | O2 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| chiatta | omp-spirv | O0 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| chiatta | omp-spirv | O2 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | hip | O0 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | hip | O2 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | omp-hip | O0 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| mi300x | omp-hip | O2 | 0 | 0 | 0 | 0 | 40 | 0/0 |
| rome | host-buffer | O0 | 36 | 31 | 5 | 4 | 0 | 9/9 |
| rome | host-buffer | O2 | 36 | 31 | 5 | 4 | 0 | 9/9 |
| rome | host-specifiers | O0 | 30 | 24 | 6 | 10 | 0 | 9/9 |
| rome | host-specifiers | O2 | 30 | 24 | 6 | 10 | 0 | 9/9 |

## Observed gaps

- **chiatta / sycl / O0:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **chiatta / sycl / O2:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **chiatta / omp-spirv / O0:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **chiatta / omp-spirv / O2:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / hip / O0:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / hip / O2:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / omp-hip / O0:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **mi300x / omp-hip / O2:** positive control failed; no conformance claim. See control diagnostics in the JSON record.
- **rome / host-buffer / O0, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **rome / host-buffer / O0, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **rome / host-buffer / O2, mismatch:** nul-char, long-padding, large-precision, percent-capacity, println-capacity.
- **rome / host-buffer / O2, unsupported:** debug-string, debug-char, long-literal, many-fields.
- **rome / host-specifiers / O0, unsupported:** signed-hex, binary-max, zero-alt, center-fill, dynamic-width, dynamic-precision, float-hex, unicode-fill, debug-string, debug-char.
- **rome / host-specifiers / O0, mismatch:** float-shortest, float-notation, float32-subnormal, float64-subnormal, unicode-width, null-pointer.
- **rome / host-specifiers / O2, unsupported:** signed-hex, binary-max, zero-alt, center-fill, dynamic-width, dynamic-precision, float-hex, unicode-fill, debug-string, debug-char.
- **rome / host-specifiers / O2, mismatch:** float-shortest, float-notation, float32-subnormal, float64-subnormal, unicode-width, null-pointer.

The companion JSON contains compiler/device identity, source file hashes, commands, per-case outcomes, and hashes/paths for raw evidence retained under artifacts/. Code coverage and performance are measured separately.
