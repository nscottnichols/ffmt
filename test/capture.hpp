#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include <ffmt/base.hpp>
// Pull in <sycl/sycl.hpp> only when the TU is built for a SYCL backend
// (so capture_stdout can take a sycl::queue&). Host test rig and OpenMP
// backends don't define SYCL_LANGUAGE_VERSION and don't have ACPP, so
// they skip this include.
#if defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP
#include <sycl/sycl.hpp>
#endif

constexpr int N = 2;

// Per-backend "this test trips a known compiler bug" gates. Default 0 here
// so SKIP_IF(FMT_xxx, "...", body) compiles cleanly on backends that don't
// hit the bug; the Makefile flips one to 1 via -DFMT_xxx=1 when needed.
//
// Bug-tag catalog (used as SKIP_IF reason strings — keep tags short, the
// test file+number plus this comment identify the specific symptom):
//   "spirv-o0"     — icpx-SYCL on SPIR64 at -O0: string-literal-through-
//                    pointer arg to printf segfaults on device.
//   "ptx-clang-o0" — clang+OpenMP+NVPTX at -O0: device printf mis-emits
//                    several arg patterns. Observed: '%s' string args
//                    truncated/mangled; leading prefix bytes of "#o"/"#x"/
//                    "#b"/sign mis-substituted in buffer-path output;
//                    sign drop on negative floats; multi-arg printfs
//                    silently drop the body; lowercase '{:a}' on float
//                    emits uppercase exponent. All clean at -O2.
//   "o2-ptx-ice"   — clang-22+OpenMP+NVPTX at -O2: OpenMPOpt's
//                    AAPointerInfoFloating asserts in Casting.h:572 when a
//                    std::format/PRINT call mixes a string-literal arg with
//                    other arg types. Preprocessor gate (not a runtime
//                    SKIP_IF) — the body must be elided to dodge the ICE.
#ifndef FFMT_PTX_CLANG_O0
#define FFMT_PTX_CLANG_O0 0
#endif
#ifndef FFMT_PTX_CLANG_O2_ICE
#define FFMT_PTX_CLANG_O2_ICE 0
#endif
#ifndef FFMT_SPIRV_O0
#define FFMT_SPIRV_O0 0
#endif

static std::string capture_stdout(auto&& fn) {
  std::cout.flush();
  fflush(stdout);

  // tmpfile() rather than Linux-only memfd_create: an anonymous, already-
  // unlinked regular file on any POSIX libc (Linux, macOS, BSD).
  FILE* tmp = std::tmpfile();
  assert(tmp);
  int mem_fd = fileno(tmp);
  int saved_fd = dup(STDOUT_FILENO);
  dup2(mem_fd, STDOUT_FILENO);

  fn();

  std::cout.flush();
  fflush(stdout);
  dup2(saved_fd, STDOUT_FILENO);
  close(saved_fd);

  auto size = lseek(mem_fd, 0, SEEK_END);
  lseek(mem_fd, 0, SEEK_SET);
  std::string result(size, '\0');
  ssize_t n = ::read(mem_fd, result.data(), size);
  assert(n == size);  // regular file: full read or EOF, no shorts
  fclose(tmp);
  return result;
}

// Split a captured stream into blocks keyed by `Test N` markers. Each block
// is the text emitted *after* that marker and before the next one. Anything
// before the first marker (e.g. setup noise) is ignored.
[[maybe_unused]] static std::map<int, std::string>
parse_blocks(const std::string& s) {
  std::map<int, std::string> out;
  std::istringstream in(s);
  std::string line;
  int id = -1;
  while (std::getline(in, line)) {
    if (line.starts_with("Test ")) {
      id = std::atoi(line.c_str() + 5);
      out.try_emplace(id);
    } else if (id >= 0) {
      out[id] += "    " + line + "\n";
    }
  }
  return out;
}

// TAP-ish per-test reporter. Parses each stream independently so a desync
// in one block doesn't bleed into later tests (previously: one shorter
// `Actual:` swallowed every passing test after it). Emits one line per
// test id and only prints the expected/actual diff on failure. Returns
// true when every test matched.
[[maybe_unused]] static bool diff_output(const char* name,
                        const std::string& expected,
                        const std::string& actual) {
  auto exp_blocks = parse_blocks(expected);
  auto act_blocks = parse_blocks(actual);
  // Union of ids, sorted ascending — std::map iterates in order; merge keys.
  std::set<int> ids;
  for (auto& [k, _] : exp_blocks) ids.insert(k);
  for (auto& [k, _] : act_blocks) ids.insert(k);
  // A block containing the line `# skipped: <reason>` (indented 4 spaces
  // by parse_blocks) is emitted by SKIP_IF on both std + target streams.
  // Surface it as TAP `skipped` rather than ok/fail.
  constexpr std::string_view skip_marker = "    # skipped: ";
  int passed = 0, failed = 0, skipped = 0;
  for (int id : ids) {
    const auto& e = exp_blocks[id];
    const auto& a = act_blocks[id];
    if (e == a && a.starts_with(skip_marker)) {
      // parse_blocks always appends '\n' to every line, so strip it
      // to keep the TAP reason line clean.
      auto reason = a.substr(skip_marker.size());
      reason.pop_back();
      fprintf(stderr, "skipped %d - %s # %s\n", id, name, reason.c_str());
      ++skipped;
    } else if (e == a) {
      fprintf(stderr, "ok %d - %s\n", id, name);
      ++passed;
    } else {
      fprintf(stderr, "not ok %d - %s\n", id, name);
      fprintf(stderr, "  Expected:\n%s", e.c_str());
      fprintf(stderr, "  Actual:\n%s", a.c_str());
      ++failed;
    }
  }
  fprintf(stderr, "# %s: %d/%zu passed", name, passed, ids.size());
  if (skipped) fprintf(stderr, " (%d skipped)", skipped);
  fprintf(stderr, "\n");
  if (failed == 0 && expected != actual) {
    fprintf(stderr, "not ok - %s: raw stream mismatch outside parsed blocks\n", name);
    return false;
  }
  return failed == 0;
}
