"""Discover feature fixtures that are present in this source revision."""
import hashlib
import importlib.util
from pathlib import Path
import sys

DIRECTORY = Path(__file__).resolve().parent
if str(DIRECTORY) not in sys.path:
    sys.path.insert(0, str(DIRECTORY))

CASES = []
NEGATIVE_CASES = []
FIXTURE_FILES = [Path(__file__), DIRECTORY / "case_support.py"]
for path in sorted(DIRECTORY.glob("cases_*.py")):
    spec = importlib.util.spec_from_file_location("ffmt_" + path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    CASES.extend(getattr(module, "CASES", []))
    NEGATIVE_CASES.extend(getattr(module, "NEGATIVE_CASES", []))
    FIXTURE_FILES.append(path)

for collection in (CASES, NEGATIVE_CASES):
    identifiers = [item["id"] for item in collection]
    if len(identifiers) != len(set(identifiers)):
        raise ValueError("conformance case identifiers must be unique")

def source_hashes():
    return {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in FIXTURE_FILES}
