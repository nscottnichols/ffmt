# Execute numeric memory records through the KOKKOS adapter

Stdout transport and floating-point memory conversion can fail independently on an accelerator.

Copy numeric input encodings to device memory, format there, then compare copied binary output records with host fmt.

Add a memory launcher to the existing independent adapter without changing other backend modules.

Runtime execution remains hardware-dependent; source-generation and host record tests validate structure but do not establish accelerator execution.

The memory launcher formats on the selected backend and copies records to the host for byte-exact oracle comparison. Stdout control failure does not hide memory outcomes.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_memory_kokkos.py
```
