# Add explicit memory and printing benchmark primitives

Formatting into memory and printing have different costs; a reusable benchmark must preserve that distinction.

Integer memory mode formats value={:+012d} into a bounded buffer; print mode emits the same presentation through the selected transport.

Keep the callable primitive, single-process sampling harness and optional second translation unit directly reviewable as handwritten C++.

These primitives define workloads, not hardware conclusions. Five warmups and thirty samples do not make first invocation a cold-JIT measurement.

Memory and print modes are separate workload primitives. Input values are exact binary fractions; shortest conversion is compared with fmt in memory only.

Validation:

```sh
'{cxx}' -std=c++20 -O2 -fno-fast-math -Iinclude test/conformance/benchmark.cpp -o '{output}/benchmark-smoke'
'{output}/benchmark-smoke' 0 0 0 1 8 1
```
