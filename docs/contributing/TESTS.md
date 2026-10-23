# Focused portable checks

Run `make CXX=clang++ FMT_DIR=/path/to/fmt test-capabilities` to compile and
run every explicitly registered capability. `make list-capabilities` lists the
required sources. Every implementation adds its own registration under
`test/conformance/suites/`; a missing registered source is a build error.

Each registration states its C++ standard, extra compiler flags and whether it
links the independent fmt reference. Pure output-accounting and parser tests do
not require fmt. Both `HOST_OPT` and `EXTRA_CXXFLAGS` remain configurable, for
example `HOST_OPT=-O0 EXTRA_CXXFLAGS=-fsanitize=address,undefined`.

Compiler-specific tests return 77 when an extension is unavailable. Make displays
that result as SKIP. A skip is not evidence that formatting conforms on that
compiler. Qualification records retain individual program exit codes.

`make conformance-harness` runs the Python tests available in the current tree.
New tooling capabilities introduce their own tests; no future tests are silently
filtered out to make an earlier intermediate tree pass.
