#!/usr/bin/env python3
"""Inspect device permissions and Intel loader variants within an allocation."""
import argparse
import datetime
import glob
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
ZE_PROBE = r"""
import ctypes,json
rows=[]
for name in ('libze_loader.so.1','libze_loader.so'):
 try:
  lib=ctypes.CDLL(name)
  init=lib.zeInit(1)
  count=ctypes.c_uint32()
  drivers=lib.zeDriverGet(ctypes.byref(count),None)
  rows.append(dict(loader=name,init=init,driver_result=drivers,driver_count=count.value))
 except Exception as e: rows.append(dict(loader=name,error=str(e)))
print(json.dumps(rows))
"""


def run(argv, env):
    try:
        result = subprocess.run(argv, capture_output=True, timeout=60, env=env)
        return dict(command=argv, returncode=result.returncode,
                    stdout=result.stdout.decode("utf-8", "replace"), stderr=result.stderr.decode("utf-8", "replace"))
    except (OSError, subprocess.SubprocessError) as exc:
        return dict(command=argv, error=str(exc))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", required=True)
    p.add_argument("--executable", help="Existing small offload control to run under every loader variant")
    a = p.parse_args()
    if not os.environ.get("COBALT_JOBID"):
        p.error("An active Cobalt allocation is required")
    base = dict(os.environ)
    parts = base.get("LD_LIBRARY_PATH", "").split(os.pathsep)
    native = dict(base)
    native["LD_LIBRARY_PATH"] = os.pathsep.join(p for p in parts if p and "intel-gpu-umd" not in p and "khronos" not in p)
    driver1032 = dict(native)
    driver1032["LD_LIBRARY_PATH"] = "/soft/libraries/intel-gpu-umd/1032.21/lib64:/soft/libraries/intel-gpu-umd/1032.21/lib64/intel-opencl:" + native["LD_LIBRARY_PATH"]
    evidence = dict(schema_version=1, profile=a.profile, hostname=socket.gethostname(),
                    cobalt_jobid=os.environ.get("COBALT_JOBID"),
                    timestamp_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    device_files={}, variants={})
    for path in glob.glob("/dev/dri/*"):
        stat = Path(path).stat()
        evidence["device_files"][path] = dict(mode=oct(stat.st_mode), uid=stat.st_uid, gid=stat.st_gid,
                                             readable=os.access(path, os.R_OK), writable=os.access(path, os.W_OK))
    evidence["kernel_drivers"] = {path: os.path.realpath(path) for path in glob.glob("/sys/class/drm/card*/device/driver")}
    for label, env in [("configured", base), ("native_driver", native), ("shared_1032", driver1032)]:
        commands = [[sys.executable, "-c", ZE_PROBE]]
        for name in ("sycl-ls", "clinfo"):
            path = shutil.which(name)
            if path:
                commands.append([path, "--verbose"] if name == "sycl-ls" else [path, "-l"])
        if a.executable:
            env["LIBOMPTARGET_DEBUG"] = "2"
            commands.append([a.executable, "0"])
        evidence["variants"][label] = dict(ld_library_path=env.get("LD_LIBRARY_PATH"),
                                            commands=[run(cmd, env) for cmd in commands])
    output = ROOT / "artifacts" / a.profile / f"diagnostics-{os.environ['COBALT_JOBID']}.json"
    output.write_text(json.dumps(evidence, indent=2) + "\n")
    print(output)


if __name__ == "__main__":
    main()
