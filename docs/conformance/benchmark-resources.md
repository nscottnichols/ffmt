# Retain compiler resource reports independently of timing measurements

Object size, static compiler frame size, accelerator scratch and registers describe different resources and must not be conflated with measured runtime.

--resources-only compiles one- and two-call probes and preserves the compiler report without claiming device execution.

Keep raw tool output, object hashes and backend image metadata beside parsed resource fields; distinguish compile-only probe outcomes from timing rows.

Tool availability and diagnostic wording vary. Static frame reports exclude cumulative call depth; linked-image and conversion-table sizes are separate measurements.

Resource probes are compile-only evidence. Compiler frames, scratch, registers, retained tables and image sizes are named separately and retain raw commands and diagnostics.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_benchmark.py
python3 tools/benchmark.py --route host-buffer --resources-only --family integer --mode memory --capacity 128
```
