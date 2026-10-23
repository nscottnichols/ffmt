"""Verify reference inputs without changing the caller's fmt checkout."""
import hashlib
from pathlib import Path
import subprocess

FMT_COMMIT = "0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f"

def oracle_identity(checkout, expected_commit=FMT_COMMIT):
    checkout = Path(checkout).resolve()
    def git(*arguments):
        try:
            return subprocess.check_output(["git", "-C", str(checkout), *arguments],
                                           text=True, stderr=subprocess.PIPE).strip()
        except (OSError, subprocess.CalledProcessError) as error:
            raise ValueError("cannot inspect fmt checkout: " + str(checkout)) from error

    commit = git("rev-parse", "HEAD")
    if commit != expected_commit:
        raise ValueError("fmt checkout differs from oracle pin: expected " + expected_commit + ", found " + commit)
    if git("status", "--porcelain"):
        raise ValueError("fmt oracle checkout must be clean; preserve local edits and select a clean checkout")
    return dict(repository="https://github.com/fmtlib/fmt", commit=commit,
                format_header_sha256=hashlib.sha256((checkout / "include/fmt/format.h").read_bytes()).hexdigest(),
                format_source_sha256=hashlib.sha256((checkout / "src/format.cc").read_bytes()).hexdigest())
