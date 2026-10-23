"""Discover independent portable execution adapters.

Adapters are handwritten Python and C++ launcher templates. Only adapters in
this revision are registered; an unknown route is never treated as host code.
"""
import importlib.util
import os
from pathlib import Path
import shlex

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = Path(os.environ.get("FFMT_WORKSPACE", ROOT)).resolve()

def base_flags(route, optimization):
    strict = [] if route == "nvcc" else ["-fno-fast-math"]
    return ["-std=c++20", "-O" + str(optimization)] + strict

ADAPTERS = {}
wrappers = []
for path in sorted(Path(__file__).parent.glob("backend_*.py")):
    spec = importlib.util.spec_from_file_location("ffmt_" + path.stem, path)
    adapter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(adapter)
    if hasattr(adapter, "BASES"):
        wrappers.append(adapter)
    else:
        for route in adapter.ROUTES:
            ADAPTERS[route] = adapter
for adapter in wrappers:
    for route, base in adapter.BASES.items():
        if base in ADAPTERS:
            ADAPTERS[route] = adapter
ROUTES = tuple(ADAPTERS)

def adapter(route):
    try:
        return ADAPTERS[route]
    except KeyError:
        raise ValueError("unknown backend route: " + route) from None

def compiler(route):
    default = adapter(route).compiler(route)
    override = os.environ.get("FFMT_CXX_" + route.upper().replace("-", "_"))
    return shlex.split(override or default)

def flags(route, optimization):
    result = adapter(route).flags(route, optimization)
    return result + shlex.split(os.environ.get("FFMT_FLAGS_" + route.upper().replace("-", "_"), ""))

def link_flags(route):
    module = adapter(route)
    return module.link_flags(route) if hasattr(module, "link_flags") else []

def prelude(route):
    return adapter(route).prelude(route)

def launcher(route):
    return adapter(route).launcher(route)
