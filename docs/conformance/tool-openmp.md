# Add independent OPENMP differential execution adapter

Portable output cases need a backend-specific execution launcher without coupling unrelated accelerator runtimes.

Reject an OpenMP target region that executes on the initial host device.

Register this adapter independently; keep handwritten compiler flags and C++ launch code next to each other.

Execution requires a working offload toolchain and accelerator; host fallback is failure.

Reject an OpenMP target region that executes on the initial host device. Execution requires a working offload toolchain and accelerator; host fallback is failure.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_adapter_openmp.py
```
