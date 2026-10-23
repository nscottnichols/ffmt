# Compiler/runtime reproducers

Independent compiler/runtime controls belong in the companion
[ffmt-evaluation](https://github.com/nscottnichols/ffmt-evaluation) repository. They do not include ffmt and do not serve
as library regressions.

The two controls inherited from the upstream library baseline are preserved at:

- [DPC++ local-pointer printf control](https://github.com/nscottnichols/ffmt-evaluation/blob/179e6e59e1a0094ec2e4f56aab11eb568f058055/implementation_limitation/dpcpp_O0.cpp).
- [Intel OpenMP pragma-in-macro control](https://github.com/nscottnichols/ffmt-evaluation/blob/179e6e59e1a0094ec2e4f56aab11eb568f058055/implementation_limitation/icpx_omp_pragma_in_dowhile.cpp).

Their original source bytes, context and provenance remain in evaluation. The
[repository split ledger](https://github.com/nscottnichols/ffmt-evaluation/blob/179e6e59e1a0094ec2e4f56aab11eb568f058055/docs/provenance/split.json)
records those paths and hashes. Removing their copies from this source tree does
not rewrite the upstream baseline or alter captured failures. Portable tests
that directly exercise ffmt remain under `test/` in this repository.
