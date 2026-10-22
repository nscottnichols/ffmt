#!/bin/bash
set -euo pipefail
profile=${1:?profile required}
revision=${2:?immutable revision required}
shift 2
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
source tools/jlse/environment.sh "$profile"
exec "${FFMT_PYTHON:-python3}" tools/jlse/build_checks.py --profile "$profile" --revision "$revision" "$@"
