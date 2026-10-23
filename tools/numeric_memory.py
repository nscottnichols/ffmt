"""Binary numeric records and handwritten C++ conformance source templates."""
from pathlib import Path
import struct
import backends

MAGIC = 0x46464D54
SEED = 0x4D595DF4D0F33173
CAPACITY = 768
CORPUS_TEMPLATE = Path(__file__).resolve().parents[1] / "test/conformance/numeric_memory.cpp.in"

def parse_records(data):
    if len(data) < 8:
        raise ValueError("missing memory-suite header")
    magic, count = struct.unpack_from("<II", data)
    if magic != MAGIC:
        raise ValueError("incorrect memory-suite magic")
    offset, records = 8, []
    for _ in range(count):
        if offset + 24 > len(data):
            raise ValueError("missing or truncated memory record")
        kind, bits, written, required, error = struct.unpack_from("<IQIII", data, offset)
        offset += 24
        if written > 768 or offset + written > len(data):
            raise ValueError("invalid or truncated memory record payload")
        records.append(dict(kind=kind, bits=bits, written=written, required=required,
                            error=error, output=data[offset:offset + written]))
        offset += written
    if offset != len(data):
        raise ValueError("extra/duplicated bytes after memory records")
    return records

def compare_records(expected_data, actual_data):
    expected, actual = parse_records(expected_data), parse_records(actual_data)
    if len(expected) != len(actual):
        raise ValueError("missing or duplicated memory records")
    failures = []
    for i, (e, a) in enumerate(zip(expected, actual)):
        if e != a:
            failures.append(dict(index=i, kind=e["kind"], bits=hex(e["bits"]),
                                 expected=e["output"].hex(), actual=a["output"].hex(),
                                 expected_required=e["required"], required=a["required"], error=a["error"]))
    return dict(status="pass" if not failures else "mismatch", input_cases=len(expected),
                passed=len(expected) - len(failures), failed=len(failures),
                seed=hex(SEED), formats_per_input=5, failures=failures)

def source(route, oracle=False, samples=4096):
    if samples < 0:
        raise ValueError("numeric sample count must be nonnegative")
    module = backends.adapter("host-buffer" if oracle else route)
    if not hasattr(module, "numeric_launcher"):
        raise ValueError("numeric memory execution is unavailable for route: " + route)
    prefix = "#define HD\n#include <fmt/format.h>\n#define ORACLE 1\n" if oracle else backends.prelude(route) + "#include <ffmt/base.hpp>\n"
    body = CORPUS_TEMPLATE.read_text().replace("SAMPLE_COUNT", str(samples))
    return prefix + body + module.numeric_launcher(route)
