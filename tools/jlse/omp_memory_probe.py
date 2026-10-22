"""Minimize teams mapping, scalar conversion, and dynamic-stack failures."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import conformance
import numeric_memory

ROOT = Path(__file__).resolve().parents[2]


def source(route, workload, oracle=False):
    name, pattern, argument, count, serial = workload
    text = numeric_memory.source(route, oracle=oracle, samples=(count-24)//2 if count > 24 else 0)
    prefix = ""
    if pattern and pattern.startswith("@deep"):
        depth, width = (int(part) for part in pattern[len("@deep"):].split("x"))
        text = text.replace("#include <ffmt/base.hpp>\n", "")
        body = "unsigned seed;if constexpr(sizeof(T)==8)seed=unsigned(__builtin_bit_cast(std::uint64_t,value));else seed=__builtin_bit_cast(std::uint32_t,value);"
        if oracle:
            body += "unsigned checksum=0;for(unsigned level=0;level<="+str(depth)+";++level){unsigned s=seed+level;checksum+=(s+s%"+str(width)+")&255;}"
        else:
            prefix = "HD __attribute__((noinline)) unsigned probe_deep_leaf(const volatile unsigned char* data,unsigned index){return data[index];}\n"
            prefix += "template<int Depth,int Width>HD __attribute__((noinline)) unsigned probe_deep(unsigned seed){volatile unsigned char local[Width];for(unsigned j=0;j<Width;++j)local[j]=static_cast<unsigned char>((seed+j)&255);unsigned tail=0;if constexpr(Depth>0)tail=probe_deep<Depth-1,Width>(seed+1);return tail+probe_deep_leaf(local,seed%Width);}\n"
            body += "unsigned checksum=probe_deep<"+str(depth)+","+str(width)+">(seed);"
        body += "out.written=out.required=4;out.error=0;for(unsigned j=0;j<4;++j)out.text[j]=char(checksum>>(8*j));"
    elif pattern == "@stack" and not oracle:
        prefix = 'HD __attribute__((noinline)) void probe_leaf(char* data){data[0]=\'x\';}\n'
        body = "char local[768];probe_leaf(local);out.text[0]=local[0];out.written=out.required=1;out.error=0;"
        text = text.replace("#include <ffmt/base.hpp>\n", "")
    elif pattern is None or pattern == "@stack":
        body = "out.text[0]='x';out.written=out.required=1;out.error=0;"
    elif oracle:
        body = 'auto value_text=fmt::format("' + pattern + '",' + argument + ');'
        body += "out.written=out.required=unsigned(value_text.size());out.error=0;for(unsigned j=0;j<out.written;++j)out.text[j]=value_text[j];"
    else:
        body = 'auto r=ffmt::format_to_n(out.text,sizeof(out.text),"' + pattern + '",' + argument + ');'
        body += "out.written=unsigned(r.written);out.required=unsigned(r.required);out.error=unsigned(r.error);"
    begin = text.index("template<class T>HD void format_record_value")
    end = text.index("HD void format_record(", begin)
    text = text[:begin] + prefix + "template<class T>HD __attribute__((noinline)) void format_record_value(T value,Output& out){" + body + "}\n" + text[end:]
    if count < 24:
        text = text.replace("auto in=inputs();", "auto in=inputs();in.resize("+str(count)+");")
    if serial == "target" and not oracle:
        text = text.replace("#pragma omp target teams distribute parallel for map", "#pragma omp target map")
        text = text.replace("for(unsigned i=0;i<n;++i)format_record(di[i],dout[i]);", "{for(unsigned i=0;i<n;++i)format_record(di[i],dout[i]);}")
    elif serial and not oracle:
        text = text.replace("target teams distribute parallel for map", "target teams distribute parallel for num_teams(1) thread_limit(1) map")
    return text


def run_probes(profile, timeout=120, large=False, loops=False, stack_teams=False, deep=False):
    route = {"mi300x": "omp-hip", "h100": "omp-cuda", "chiatta": "omp-spirv"}[profile]
    directory = ROOT / "artifacts" / profile / ("omp-memory-" + os.environ["COBALT_JOBID"])
    directory.mkdir(parents=True, exist_ok=True)
    manifest = dict(schema_version=1, profile=profile, route=route,
                    source_files=conformance.snapshot(directory / "source", "WORKTREE"),
                    generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), tests=[])
    fmt = ROOT / "third_party/fmt"
    manifest["fmt_commit"] = conformance.git("-C", str(fmt), "rev-parse", "HEAD")
    if manifest["fmt_commit"] != conformance.CONFIG["fmt_commit"]:
        raise ValueError("The numeric oracle must use the campaign fmt pin")
    host = shlex.split(os.getenv("FFMT_HOST_CXX", os.getenv("CXX", "g++")))
    common = ["-std=c++20", "-O2", "-fno-fast-math", "-I"+str(fmt/"include")]
    build = conformance.run([*host, *common, "-c", str(fmt/"src/format.cc"), "-o", str(directory/"fmt.o")], directory, "fmt-build", 300)
    manifest["fmt_build"] = conformance.record(build)
    if build["exit_code"]:
        return manifest
    full = "{}|{:.6f}|{:.5e}|{:.17g}|{:#020.5a}"
    workloads = [
        ("mapped-write", None, "", 24, False),
        ("integer", "{}", "7", 24, False),
        ("shortest", "{}", "value", 24, False),
        ("fixed", "{:.6f}", "value", 24, False),
        ("full-one", full, "value,value,value,value,value", 1, False),
        ("full-specials", full, "value,value,value,value,value", 24, False),
        ("full-random", full, "value,value,value,value,value", 56, False),
        ("full-one-team", full, "value,value,value,value,value", 24, True),
    ]
    if large:
        workloads = [
            ("mapped-large", None, "", 8216, False),
            ("integer-large", "{}", "7", 8216, False),
            ("shortest-large", "{}", "value", 8216, False),
            ("fixed-large", "{:.6f}", "value", 8216, False),
            ("full-large", full, "value,value,value,value,value", 8216, False),
        ]
    if loops:
        workloads = [("integer-target-"+str(n), "{}", "7", n, "target") for n in (1, 2, 4)]
        workloads += [("stack-target-"+str(n), "@stack", "", n, "target") for n in (1, 2, 4)]
    if stack_teams:
        workloads = [("stack-teams-"+str(n), "@stack", "", n, False) for n in (1, 24, 8216)]
        workloads += [("stack-one-team-24", "@stack", "", 24, True)]
    if deep:
        workloads = []
        for depth in (4, 16, 64):
            for width in (64, 768):
                pattern = "@deep"+str(depth)+"x"+str(width)
                workloads += [("deep-teams-"+str(depth)+"x"+str(width), pattern, "", 8216, False),
                              ("deep-target-"+str(depth)+"x"+str(width), pattern, "", 4, "target")]
    for workload in workloads:
        name = workload[0]
        oracle = directory / (name+"-oracle.cpp")
        oracle.write_text(source("host-buffer", workload, True))
        build = conformance.run([*host, *common, str(oracle), str(directory/"fmt.o"), "-o", str(oracle.with_suffix(""))], directory, name+"-oracle-build", 300)
        if build["exit_code"]:
            manifest["tests"].append(dict(name=name, status="oracle_compile_failure", build=conformance.record(build)))
            continue
        expected = conformance.run([str(oracle.with_suffix(""))], directory, name+"-oracle-run", timeout)
        for opt in (0, 2):
            label = name+"-O"+str(opt)
            fixture = directory / (label+".cpp")
            fixture.write_text(source(route, workload))
            build = conformance.run([*conformance.compiler(route), *conformance.flags(route,opt),
                                     "-I"+str(directory/"source/include"), str(fixture), "-o", str(fixture.with_suffix(""))], directory, label+"-build", 300)
            row = dict(name=name, opt=opt, source_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),
                       build=conformance.record(build), status="compile_failure")
            if build["exit_code"] == 0:
                execution = conformance.run([str(fixture.with_suffix(""))], directory, label+"-run", timeout)
                row.update(execution=conformance.record(execution),status="execution_failure")
                if expected["exit_code"] == 0 and execution["exit_code"] == 0:
                    try:
                        row.update(numeric_memory.compare_records(expected["stdout"], execution["stdout"]))
                    except ValueError as exc:
                        row.update(status="malformed_output",error=str(exc))
            manifest["tests"].append(row)
            (directory/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
            print("OpenMP memory",label,row["status"],flush=True)
    return manifest
