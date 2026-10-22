These standalone reproducers preserve compiler/runtime failures separately from
formatting conformance results. They are not enabled as passing tests by default.

`amd_omp_callback.cpp` reproduces an OpenMP callback failure without including
fmt or ffmt. It writes one byte through nested callbacks for each of 8,216
independent records. Exit 77 rejects host fallback; success prints `PASS8216`.
The source is preserved byte for byte from the tested artifact; its SHA-256 and
compact observations are in [amd_omp_callback.json](amd_omp_callback.json).

Observed on MI300X (`gfx942`), matched ROCm 7.0.2 compiler/runtime, AMD Clang
20.0.0git (`0dda3adf56766e0aac0d03173ced3759e1ffecbc`):

| Configuration | Observation |
| --- | --- |
| Native `-O0` | Compiles; GPU null-address memory fault, SIGABRT |
| Native `-O2` | Compiles; exact `PASS8216`, exit 0 |
| GCC 13.3 host, `-O1`, ASan/UBSan | Exact `PASS8216`, no sanitizer diagnostics |

These results identify a compiler/runtime boundary; the responsible compiler
pass or runtime routine is unresolved. Simple stack-call controls also passed.
No formatting-library workaround was applied. OpenMP specifies implicit device
availability for [procedures referenced in target regions](https://www.openmp.org/spec-html/5.2/openmpse85.html).

The identical tracked source was also run with Clang 22.1.8 OpenMP on NVIDIA
A40 (`sm_86`): **both `-O0` and `-O2` pass**. Its commands, hashes and discovery
are in [nvidia_omp_callback.json](nvidia_omp_callback.json). It does not reproduce
the separate NVIDIA `-O0` numeric-memory illegal access, whose cause remains
unresolved. Do not generalize the AMD callback failure to that NVIDIA result.

Run native checks inside an AMD GPU allocation, using its matching ROCm paths:

```sh
mkdir -p build/regressions
ffmt_rocm=/soft/compilers/rocm/rocm-7.0.2
"$ffmt_rocm/llvm/bin/clang++" -std=c++20 -O0 -fno-fast-math -fopenmp \
  --offload-arch=gfx942 --rocm-path="$ffmt_rocm" \
  test/regressions/amd_omp_callback.cpp -o build/regressions/amd_omp_callback_O0
OMP_TARGET_OFFLOAD=MANDATORY \
  LD_LIBRARY_PATH="$ffmt_rocm/llvm/lib:$ffmt_rocm/lib:${LD_LIBRARY_PATH:-}" \
  build/regressions/amd_omp_callback_O0
```

Repeat with `-O2` and a separate output binary. For the host sanitizer control,
generate the tested source variant by reversing only the initial-device gate:

```sh
python3 - <<'PY'
from pathlib import Path
source = Path('test/regressions/amd_omp_callback.cpp').read_text()
assert source.count('if(initial)return 77;') == 1
Path('build/regressions/amd_omp_callback_host.cpp').write_text(
    source.replace('if(initial)return 77;', 'if(initial!=1)return 77;'))
PY
g++ -std=c++20 -O1 -g -fno-fast-math -fopenmp -foffload=disable \
  -fsanitize=address,undefined build/regressions/amd_omp_callback_host.cpp \
  -o build/regressions/amd_omp_callback_host
OMP_TARGET_OFFLOAD=DISABLED OMP_NUM_THREADS=4 \
  build/regressions/amd_omp_callback_host
```
