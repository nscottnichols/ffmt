#!/bin/bash
# Build the working paper from tracked, explicitly selected result manifests.
set -euo pipefail
ffmt_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export TEXMFHOME="$ffmt_root/software/local/texmf"
if [ "${1:-}" = --install-tex ]; then
  if [ ! -f "$TEXMFHOME/tlpkg/texlive.tlpdb" ]; then
    tlmgr --usermode --usertree "$TEXMFHOME" init-usertree
  fi
  tlmgr --usermode --usertree "$TEXMFHOME" install microtype amsmath booktabs tools geometry xcolor listings xurl hyperref
  shift
fi
if [ "$#" -ne 0 ]; then
  echo 'usage: tools/build_paper.sh [--install-tex]' >&2
  exit 2
fi
cd "$ffmt_root"
python3 tools/paper_tables.py --qualification docs/results/qualification.json \
  --benchmarks docs/results/benchmarks.json --coverage docs/results/coverage.json \
  --output docs/paper/results.tex --selection-output docs/results/paper-selection.json
cd docs/paper
pdflatex -interaction=nonstopmode -halt-on-error ffmt.tex
pdflatex -interaction=nonstopmode -halt-on-error ffmt.tex
