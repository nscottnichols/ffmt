# Working manuscript

`ffmt.tex` revises the supplied outline for the implemented shared engine and
the captured JLSE experiments. It is a working paper, not a submitted artifact.
Qualification tables are generated from explicitly selected evidence; missing
measurements and failing configurations retain their status.

`outline-v2.tex` preserves the supplied 21 October source as historical input.
Its statements about the old two-path implementation and unexecuted tests are
not descriptions of the current implementation. Input SHA256:
`519add4175236b66b5c52300b6a7f00a0accec5bc4894a894eaea7911dcd73f0`.

Build from the repository root after exporting the compact evidence:

```sh
tools/build_paper.sh
```

This requires an existing TeX Live installation with `pdflatex`. If its optional
packages are missing, use `tools/build_paper.sh --install-tex`; packages install
into ignored `software/local/texmf`. The helper uses that directory as
`TEXMFHOME`. Tables and `docs/results/paper-selection.json` are regenerated
together. The selection ledger identifies every displayed timing row and its
header fingerprint; the full benchmark JSON retains earlier snapshots.

The committed reading copy is `ffmt.pdf`. Raw logs, binaries and installations
remain under ignored artifacts/software directories. The JSON evidence retains
source hashes and command identities; numbers in this paper describe those
captured snapshots and their stated measurement conditions.
