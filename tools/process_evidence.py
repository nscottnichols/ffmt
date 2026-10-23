"""Capture exact process evidence and terminate timed-out process groups."""
import base64
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]

def path_label(path):
    """Keep historical relative paths, while allowing an external workspace."""
    path = Path(path).resolve()
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)

def compare_bytes(expected, actual):
    if expected == actual:
        return dict(status="pass", expected_bytes=len(expected), actual_bytes=len(actual))
    offset = next((i for i, (a, b) in enumerate(zip(expected, actual)) if a != b), min(len(expected), len(actual)))
    return dict(status="mismatch", first_difference=offset,
                symptom="truncated" if expected.startswith(actual) else "different_bytes",
                expected_bytes=len(expected), actual_bytes=len(actual),
                expected_base64=base64.b64encode(expected).decode("ascii"),
                actual_base64=base64.b64encode(actual).decode("ascii"))

def run(command, directory, stem, timeout=180):
    directory.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    try:
        with subprocess.Popen([str(x) for x in command], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, cwd=str(ROOT),
                              start_new_session=True) as p:
            try:
                stdout, stderr = p.communicate(timeout=timeout)
                result = dict(exit_code=p.returncode, stdout=stdout, stderr=stderr)
            except subprocess.TimeoutExpired:
                # Compiler drivers and JIT launchers can have descendants that
                # keep running and hold these pipes open after the parent dies.
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                stdout, stderr = p.communicate()
                result = dict(exit_code=-1, stdout=stdout, stderr=stderr, timeout=True)
    except OSError as e:
        result = dict(exit_code=-1, stdout=b"", stderr=str(e).encode())
    result.update(command=[str(x) for x in command], seconds=time.monotonic() - start)
    for stream in ("stdout", "stderr"):
        path = directory / (stem + "." + stream)
        path.write_bytes(result[stream])
        result[stream + "_file"] = path_label(path)
    return result

def record(p):
    return {k: v for k, v in p.items() if k not in ("stdout", "stderr")}
