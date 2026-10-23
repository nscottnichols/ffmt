# Add independent CUDA differential execution adapter

Portable output cases need a backend-specific execution launcher without coupling unrelated accelerator runtimes.

Run a literal case in a one-thread CUDA kernel through nvcc or Clang.

Register this adapter independently; keep handwritten compiler flags and C++ launch code next to each other.

Execution requires a CUDA device and runtime; Python checks only source generation here.

Run a literal case in a one-thread CUDA kernel through nvcc or Clang. Execution requires a CUDA device and runtime; Python checks only source generation here.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_adapter_cuda.py
```
