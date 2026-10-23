# Add independent SYCL differential execution adapter

Portable output cases need a backend-specific execution launcher without coupling unrelated accelerator runtimes.

Use a GPU selector, launch one SYCL task, and retain its device name.

Register this adapter independently; keep handwritten compiler flags and C++ launch code next to each other.

Execution requires SYCL or AdaptiveCpp GPU support; a CPU fallback cannot pass.

Use a GPU selector, launch one SYCL task, and retain its device name. Execution requires SYCL or AdaptiveCpp GPU support; a CPU fallback cannot pass.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_adapter_sycl.py
```
