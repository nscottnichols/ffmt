# Add independent KOKKOS differential execution adapter

Portable output cases need a backend-specific execution launcher without coupling unrelated accelerator runtimes.

Only advertise kokkos-cuda when the CUDA base adapter is present.

Register this adapter independently; keep handwritten compiler flags and C++ launch code next to each other.

Kokkos must be installed externally with the selected execution space; no site profiles are included.

Only advertise kokkos-cuda when the CUDA base adapter is present. Kokkos must be installed externally with the selected execution space; no site profiles are included.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_adapter_kokkos.py
```
