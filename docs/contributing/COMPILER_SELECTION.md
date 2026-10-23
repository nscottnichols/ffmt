# Compiler selection

`USE_ACPP=1` selects `acpp` from the caller PATH. The Makefile does not prepend a developer-specific installation path. Load a compiler environment or pass the compiler as a Make command-line variable: `make USE_ACPP=1 CXX=/path/to/acpp`. An environment-only `CXX` assignment is overridden by this backend's compiler selection. Site provisioning remains in ffmt-evaluation.
