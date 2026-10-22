#!/bin/bash
set -euo pipefail
# Cobalt's SSH launcher does not preserve spaces inside environment values.
# Native /usr/bin/python3 reads the validated non-secret compiler selections,
# then replaces this process with the same job script and the exact environment.
if [ -n "${FFMT_JOB_ENV_FILE:-}" ]; then
  exec /usr/bin/python3 -c 'import json,os,sys
environment=os.environ.copy()
with open(sys.argv[1]) as stream: environment.update(json.load(stream))
environment.pop("FFMT_JOB_ENV_FILE",None)
os.execve("/bin/bash",["/bin/bash"]+sys.argv[2:],environment)' "$FFMT_JOB_ENV_FILE" "$0" "$@"
fi
profile=${1:?system profile required}
stage=${2:-baseline}
revision=${3:-}
shift "$(( $# >= 3 ? 3 : $# ))"
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
if [ -z "$revision" ]; then
  if [ "$stage" = baseline ]; then
    revision=3fe258a87675c997667c899c2b0f0cad405aad5a
  else
    revision=$(git rev-parse --verify 'HEAD^{commit}')
  fi
fi
mkdir -p "software/$profile/"{downloads,src,build,install} "artifacts/$profile"
exec > >(tee -a "artifacts/$profile/job-${COBALT_JOBID:-manual}.log") 2>&1
printf 'profile=%s stage=%s revision=%s\n' "$profile" "$stage" "$revision"
date -u
hostname
uname -a
source tools/jlse/environment.sh "$profile"
ffmt_python=${FFMT_PYTHON:-python3}
if ! "$ffmt_python" -c 'import sys' >/dev/null 2>&1; then ffmt_python=/usr/bin/python3; fi
export FFMT_PYTHON="$ffmt_python"
"$ffmt_python" tools/jlse/discover.py --profile "$profile" --output "artifacts/$profile/discovery-${COBALT_JOBID:-manual}.json"
if [ "$stage" = discovery ]; then exit 0; fi
case "$stage" in
  host-tests) exec "$ffmt_python" tools/jlse/host_tests.py --profile "$profile" --revision "$revision" "$@" ;;
  diagnostics) exec "$ffmt_python" tools/jlse/diagnose.py --profile "$profile" "$@" ;;
  provision) exec "$ffmt_python" tools/jlse/provision.py --profile "$profile" "$@" ;;
  benchmark) exec "$ffmt_python" tools/benchmark.py --profile "$profile" --revision "$revision" "$@" ;;
  *) exec "$ffmt_python" tools/conformance.py --profile "$profile" --revision "$revision" --label "$stage" "$@" ;;
esac
