SHELL    := /bin/bash
# -fno-fast-math: keep float subnormals intact. icpx's CRT enables
# FTZ/DAZ at startup whenever any TU is compiled at -O1+, which makes
# std::format/printf flush subnormals to zero on the host while the
# SYCL device runtime does not — breaking the host-reference diff for
# values like float denorm_min. Disabling fast-math keeps host and
# device agreeing across icpx/gcc/clang.
CXXFLAGS := -std=c++20 -Wall -Werror -fno-fast-math -Iinclude
# CI / sanitizer injection: append to CXXFLAGS without disturbing the base
# flags above (e.g. EXTRA_CXXFLAGS="-fsanitize=address,undefined").
CXXFLAGS += $(EXTRA_CXXFLAGS)

# Nanosecond clock for the per-test timings. Not `date +%s%N`: BSD/macOS
# date may not support %N.
NOW_NS := perl -MTime::HiRes=time -e 'printf("%d\n", time() * 1e9)'

# Test reference: {fmt}, pinned so every machine diffs against the same
# output (the system std::format varies: libc++ and libstdc++ disagree on
# some corners). Point FMT_DIR at an existing checkout to skip the fetch.
FMT_REPO ?= https://github.com/fmtlib/fmt.git
FMT_REF  ?= 0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f
FMT_DIR  ?= third_party/fmt
FMT_HDR  := $(FMT_DIR)/include/fmt/format.h
# Not header-only: its implementation is compiled once into a host object
# (FMT_OBJ below), so offload device passes never parse it. Header-only
# {fmt} trips llvm/llvm-project#52699 under OpenMP offloading
# (-fopenmp-targets / --offload-arch): "explicit specialization of 'do_put'
# after instantiation" in format-inl.h.
CXXFLAGS += -isystem $(FMT_DIR)/include
FMT_OBJ  := build/fmt.o

# Opt level for host build (test-host + coverage). Overridable from CI
# so the matrix can exercise -O0 / -O2 (or -O3, etc.). Don't put this
# in CXXFLAGS — the device builds have their own opt levels.
HOST_OPT ?= -O2

# icpx OpenMP offload targets (USE_OMP_ICPX, and readme3_sycl_omp under
# icpx -fsycl). AOT_DEVICE=pvc (any `ocloc -device` name) compiles the
# SPIR-V to that device's ISA at build time, here and for -fsycl, so IGC's
# codegen is checked without a GPU.
ICPX_OMP_TARGETS := $(if $(AOT_DEVICE),spir64_gen -Xopenmp-target-backend "-device $(AOT_DEVICE)",spir64)

# Test sources are test/test_<name>.cpp. TESTS build on both paths;
# BUFFER_TESTS use buffer-path-only specs ({:a}, {:b}, {:^}, ...), which the
# specifiers path rejects at compile time.
TESTS        := integers floats strings layout misc formatter
BUFFER_TESTS := buffer_path
TEST_HDRS    := $(wildcard test/*.hpp test/*.inc)

# The -D selecting each path, for the host builds, which compile both
# (device builds get their path from the compiler, see base.hpp).
FLAGS_specifiers := -DFFMT_BUFFER_PATH=0
FLAGS_buffer     := -DFFMT_BUFFER_PATH=1

# ── Backend ─────────────────────────────────────────────────
# Each backend sets the compiler and flags, the tests it builds
# (DEVICE_TESTS), its examples, and what `make` runs (TEST_GOAL).
# `origin` checks (not `?=`): Make's built-in default `CXX=g++` counts as
# "set" for `?=`, which would leave gcc in place; an env- or
# command-line-supplied CXX (e.g. CXX=g++ from CI) still wins.
ifdef USE_ACPP
  export PATH := $(HOME)/projet/p26.02/install/bin:$(PATH)
  CXX             := acpp
  BACKEND_FLAGS   := --acpp-targets=generic
  OPT_LEVELS      := O0 O2
  DEVICE_TESTS    := $(TESTS) $(BUFFER_TESTS) escape_percent
  EXAMPLES        := build/readme1_sycl build/readme2_sycl
  TEST_GOAL       := test
else ifdef USE_OMP_CLANG
  # Mainline LLVM clang offloading to NVIDIA via NVPTX. The header's
  # __clang__ && _OPENMP block auto-installs the buffer-path emit hook
  # (printf("%s", out.data)) — no extra defines needed.
  ifeq ($(origin CXX),default)
    CXX           := clang++
  endif
  OFFLOAD_ARCH    ?= sm_80
  # Two known clang-22+NVPTX bugs, gated per opt level:
  #   -O0  → printf emit bugs (FFMT_PTX_CLANG_O0, runtime SKIP_IF).
  #   -O2  → OpenMPOpt ICE on two specific format calls
  #          (FFMT_PTX_CLANG_O2_ICE, compile-time #if gate).
  # See capture.hpp for the catalog; tests gate accordingly.
  OMP_OPT         ?= -O2
  BACKEND_FLAGS   := -fopenmp --offload-arch=$(OFFLOAD_ARCH) $(OMP_OPT)
  ifeq ($(OMP_OPT),-O0)
    BACKEND_FLAGS += -DFFMT_PTX_CLANG_O0
  endif
  ifeq ($(OMP_OPT),-O2)
    BACKEND_FLAGS += -DFFMT_PTX_CLANG_O2_ICE
  endif
  OPT_LEVELS      := O0 O2
  DEVICE_TESTS    := $(TESTS) $(BUFFER_TESTS)
  EXAMPLES        := build/readme1_omp build/readme2_omp
  TEST_GOAL       := test-omp
else ifdef USE_CUDA_CLANG
  # clang in CUDA mode targeting NVIDIA. `-x cuda` treats the input as CUDA
  # regardless of extension so we can share .cpp test sources with the SYCL
  # rig. The header's __CUDACC__ branch installs the same buffer-path emit
  # hook as clang-OMP-CUDA. cudart is needed to link the runtime stubs.
  ifeq ($(origin CXX),default)
    CXX           := clang++
  endif
  CUDA_ARCH       ?= sm_80
  CUDA_PATH       ?= /usr/local/cuda
  BACKEND_FLAGS   := -x cuda --cuda-gpu-arch=$(CUDA_ARCH) \
                     -L$(CUDA_PATH)/lib64 -lcudart
  OPT_LEVELS      := O0 O2
  DEVICE_TESTS    := $(TESTS) $(BUFFER_TESTS)
  EXAMPLES        := build/readme1_cuda build/readme2_cuda
  TEST_GOAL       := test-cuda
else ifdef USE_OMP_ICPX
  # icpx OpenMP-target on SPIR64. Shares the SPIR backend with icpx-SYCL,
  # so this hits the specifiers path; the header's __INTEL_LLVM_COMPILER
  # && _OPENMP block auto-installs the omp_printf → __spirv_ocl_printf
  # emit hook.
  ifeq ($(origin CXX),default)
    CXX           := icpx
  endif
  OMP_OPT         ?= -O0
  BACKEND_FLAGS   := -fiopenmp -fopenmp-targets=$(ICPX_OMP_TARGETS) $(OMP_OPT)
  # Same SPIRV-O0 string-literal-through-pointer bug the SYCL path hits.
  ifeq ($(OMP_OPT),-O0)
    BACKEND_FLAGS += -DFFMT_SPIRV_O0
  endif
  OPT_LEVELS      := O0 O2
  DEVICE_TESTS    := $(TESTS)
  EXAMPLES        := build/readme1_omp build/readme2_omp
  TEST_GOAL       := test-omp
else
  # Default: icpx -fsycl (DPC++), specifiers path.
  ifeq ($(origin CXX),default)
    CXX           := icpx
  endif
  BACKEND_FLAGS   := -fsycl $(if $(AOT_DEVICE),-fsycl-targets=spir64_gen -Xsycl-target-backend "-device $(AOT_DEVICE)")
  OPT_LEVELS      := O0 O1 O2 O3
  # FFMT_SPIRV_O0 gates tests broken by an icpx-SYCL-on-SPIR64 bug at -O0:
  # a string-literal accessed through a pointer segfaults on device.
  WA_O0           := -DFFMT_SPIRV_O0
  DEVICE_TESTS    := $(TESTS)
  EXAMPLES        := build/readme1_sycl build/readme2_sycl build/readme3_sycl_omp
  TEST_GOAL       := test
endif

.PHONY: all build test test-format examples readme-examples test-omp compile-omp \
        test-cuda test-host test-fast-math test-negative coverage clean FORCE

all: $(TEST_GOAL)

# ── Common ──────────────────────────────────────────────────

# Order-only prereq is a stamp file, not `build/`: GNU Make 3.81 (the
# macOS default) strips the trailing slash and resolves `build/` to the
# phony `build` target below, dragging every SYCL binary into test-host.
build/.dir:
	mkdir -p build
	touch $@

# Rewritten only when the compiler or flags change, and a prerequisite of
# every build: `CXX=g++ make ...` after `CXX=icpx make ...` (or another
# HOST_OPT / EXTRA_CXXFLAGS) rebuilds instead of silently rerunning the old
# binaries.
FLAGS_STAMP := build/.flags
FLAGS_NOW = $(CXX) $(CXXFLAGS) $(BACKEND_FLAGS) $(HOST_OPT)
$(FLAGS_STAMP): FORCE | build/.dir
	@echo '$(FLAGS_NOW)' | cmp -s - $@ || echo '$(FLAGS_NOW)' > $@
FORCE:

$(FMT_HDR):
	git clone --quiet $(FMT_REPO) $(FMT_DIR)
	git -C $(FMT_DIR) checkout --quiet $(FMT_REF)

# Host only (no BACKEND_FLAGS) and without -Werror: third-party code.
$(FMT_OBJ): $(FLAGS_STAMP) | build/.dir $(FMT_HDR)
	$(CXX) $(filter-out -Werror,$(CXXFLAGS)) $(HOST_OPT) -c $(FMT_DIR)/src/format.cc -o $@

DEPS := $(TEST_HDRS) include/ffmt/base.hpp $(FLAGS_STAMP)

# Run each binary in $(1), printing "<name>: PASS|FAIL (<ms>ms)"; fails if
# any failed, after running them all.
define run
@fail=0; for bin in $(1); do \
  t0=$$($(NOW_NS)); ./$$bin; rc=$$?; \
  ms=$$(( ($$($(NOW_NS)) - t0) / 1000000 )); \
  if [ $$rc -eq 0 ]; then echo "$${bin#build/}: PASS ($${ms}ms)"; \
  else echo "$${bin#build/}: FAIL ($${ms}ms)"; fail=1; fi; \
done; exit $$fail
endef

# ── Device tests: one binary per test × opt level (`make test`) ──
# `-x none` ends a `-x cuda` from BACKEND_FLAGS before the object file.

define TEST_template
build/test_$(1)_$(2): test/test_$(1).cpp $(DEPS) $(FMT_OBJ) | build/.dir $(FMT_HDR)
	$$(CXX) $$(CXXFLAGS) $$(BACKEND_FLAGS) -$(2) $$(WA_$(2)) $$< -x none $(FMT_OBJ) -o $$@
endef
$(foreach t,$(DEVICE_TESTS),$(foreach o,$(OPT_LEVELS),$(eval $(call TEST_template,$(t),$(o)))))

TEST_BINS := $(foreach t,$(DEVICE_TESTS),$(foreach o,$(OPT_LEVELS),build/test_$(t)_$(o)))

build: $(TEST_BINS)

test-format: $(TEST_BINS)
	$(call run,$^)

test: test-format readme-examples
	@echo "All tests passed."

# ── Device tests: every test in one binary (`make test-omp` / `test-cuda`) ──
# test_main_omp.cpp calls each test_X(); one GPU launch per test.

DEVICE_SRCS := test/test_main_omp.cpp $(foreach t,$(DEVICE_TESTS),test/test_$(t).cpp)

build/test_main_omp build/test_main_cuda: $(DEVICE_SRCS) $(DEPS) $(FMT_OBJ) | build/.dir $(FMT_HDR)
	$(CXX) $(CXXFLAGS) $(BACKEND_FLAGS) -DTEST_NO_MAIN $(DEVICE_SRCS) -x none $(FMT_OBJ) -o $@

test-omp: build/test_main_omp
	$(call run,$^)

test-cuda: build/test_main_cuda
	$(call run,$^)

# The same sources and the OpenMP examples, compiled (-c) but not linked:
# device CI's clang OpenMP NVPTX check. apt.llvm.org ships no NVPTX OpenMP
# device runtime, and clang also links its bitcode (libomptarget-nvptx.bc)
# into the device code at compile time; -nogpulib skips it, leaving its
# calls unresolved for the link.
COMPILE_OMP_FLAGS := $(if $(USE_OMP_CLANG),-nogpulib)

build/omp_%.o: test/test_%.cpp $(DEPS) | build/.dir $(FMT_HDR)
	$(CXX) $(CXXFLAGS) $(BACKEND_FLAGS) $(COMPILE_OMP_FLAGS) -DTEST_NO_MAIN -c $< -o $@

build/%.o: examples/%.cpp $(DEPS) | build/.dir
	$(CXX) $(CXXFLAGS) $(BACKEND_FLAGS) $(COMPILE_OMP_FLAGS) -c $< -o $@

compile-omp: $(patsubst %,build/omp_%.o,main_omp $(DEVICE_TESTS)) $(EXAMPLES:%=%.o)

# ── Examples (`make examples`, run by `make readme-examples`) ──

build/readme%: examples/readme%.cpp $(DEPS) | build/.dir
	$(CXX) $(CXXFLAGS) $(BACKEND_FLAGS) $(EXAMPLE_FLAGS) $< -o $@

build/readme%: examples/readme%.cu $(DEPS) | build/.dir
	$(CXX) $(CXXFLAGS) $(BACKEND_FLAGS) $(EXAMPLE_FLAGS) $< -o $@

# SYCL and OpenMP offload in one TU (icpx).
build/readme3_sycl_omp: EXAMPLE_FLAGS = -fiopenmp -fopenmp-targets=$(ICPX_OMP_TARGETS)

examples: $(EXAMPLES)

readme-examples: $(EXAMPLES)
	$(call run,$^)

# ── Host tests (`make test-host`, `make coverage`) ──────────
# The tests on the CPU, through libc printf, once per path: the std vs ffmt
# diff inside each test_X() is meaningful on both. build/<prefix>_<path>
# links test_main_host.cpp and one object per test.
# $(1) = prefix (test_main_host, cov), $(2) = extra flags, $(3) = path.

HOST_TESTS_specifiers := $(TESTS)
HOST_TESTS_buffer     := $(TESTS) $(BUFFER_TESTS)

define HOST_template
build/$(1)_$(3)_%.o: test/test_%.cpp $(DEPS) | build/.dir $(FMT_HDR)
	$$(CXX) $$(CXXFLAGS) $(HOST_OPT) $(2) $(FLAGS_$(3)) -DTEST_NO_MAIN -c $$< -o $$@

build/$(1)_$(3): $(foreach t,main_host $(HOST_TESTS_$(3)),build/$(1)_$(3)_$(t).o) $(FMT_OBJ)
	$$(CXX) $$(CXXFLAGS) $(2) $$^ -o $$@
endef
$(foreach p,specifiers buffer,$(eval $(call HOST_template,test_main_host,,$(p))))

test-host: build/test_main_host_specifiers build/test_main_host_buffer
	$(call run,$^)

# DAZ (denormals-are-zero, on by default with icpx and with -ffast-math on
# x86) makes a subnormal compare equal to 0.0. test_fast_math.cpp checks
# that subnormals still print exactly on both paths.
FAST_MATH_CXXFLAGS := $(filter-out -fno-fast-math,$(CXXFLAGS)) -ffast-math

build/test_fast_math_%: test/test_fast_math.cpp $(DEPS) $(FMT_OBJ) | build/.dir $(FMT_HDR)
	$(CXX) $(FAST_MATH_CXXFLAGS) $(FLAGS_$*) $(HOST_OPT) $< $(FMT_OBJ) -o $@

test-fast-math: build/test_fast_math_specifiers build/test_fast_math_buffer
	$(call run,$^)

# Region coverage of base.hpp, as the union over both paths' binaries: the
# number CI gates on (see .github/scripts/coverage_union.py for why not
# `llvm-cov -object`).
COV_FLAGS     := -fprofile-instr-generate -fcoverage-mapping
LLVM_PROFDATA  = $(shell $(CXX) -print-prog-name=llvm-profdata)
LLVM_COV       = $(shell $(CXX) -print-prog-name=llvm-cov)

$(foreach p,specifiers buffer,$(eval $(call HOST_template,cov,$(COV_FLAGS),$(p))))

coverage: build/cov_specifiers build/cov_buffer
	python3 .github/scripts/coverage_union.py $(LLVM_COV) $(LLVM_PROFDATA) include/ffmt/base.hpp $^

# ── Negative tests (`make test-negative`) ───────────────────
# Each .cpp in test/negative/ uses NEG_EXPECT_REJECTED(fmt, ...) and is
# compiled THREE times, all of which must fail to compile: on each ffmt
# path (FLAGS_specifiers, FLAGS_buffer) and with fmt::format (FLAGS_fmt;
# {fmt} checks format strings at compile time in C++20).
# The {fmt} build guards against "wrong" negative tests: a restriction we
# invent that {fmt}, our output reference, would actually accept.
# test/negative/control/accept_valid.cpp must instead compile in every
# variant, and each test must compile with -DNEG_SYNTAX_CHECK (the format
# call reduced to its arguments) — otherwise a broken harness or a typo in a
# test would make it "pass".
NEG_NAMES   := $(basename $(notdir $(wildcard test/negative/*.cpp)))
NEG_CONTROL := test/negative/control/accept_valid.cpp
FLAGS_fmt   := -DNEG_CHECK_FMT

# One make target per compile, so `make -j test-negative` runs them in
# parallel. They are never files, so they always run.
neg-control/%: $(FMT_HDR) verify-fmt
	@if $(CXX) $(CXXFLAGS) $(FLAGS_$*) -c $(NEG_CONTROL) -o /dev/null; then \
	  echo "negative/control [$*]: PASS (valid call compiles)"; \
	else echo "negative/control [$*]: FAIL (harness cannot compile a valid call)"; exit 1; fi

define NEG_template
neg/$(1)/syntax: $(FMT_HDR) verify-fmt
	@$$(CXX) $$(CXXFLAGS) -DNEG_SYNTAX_CHECK -c test/negative/$(1).cpp -o /dev/null || \
	  { echo "negative/$(1): FAIL (the test file itself does not compile)"; exit 1; }
neg/$(1)/%: neg/$(1)/syntax
	@if $$(CXX) $$(CXXFLAGS) $$(FLAGS_$$*) -c test/negative/$(1).cpp -o /dev/null >/dev/null 2>&1; then \
	  echo "negative/$(1) [$$*]: FAIL (compile unexpectedly succeeded)"; exit 1; \
	else echo "negative/$(1) [$$*]: PASS (compile rejected)"; fi
endef
$(foreach n,$(NEG_NAMES),$(eval $(call NEG_template,$(n))))

test-negative: $(foreach v,specifiers buffer fmt,neg-control/$(v)) \
               $(foreach n,$(NEG_NAMES),$(foreach v,specifiers buffer fmt,neg/$(n)/$(v)))

clean:
	rm -rf build/

# Verify, never modify, an existing oracle checkout before using its code.
.PHONY: verify-fmt
verify-fmt: $(FMT_HDR)
	@expected=$$(git -C "$(FMT_DIR)" rev-parse --verify '$(FMT_REF)^{commit}') && \
	 actual=$$(git -C "$(FMT_DIR)" rev-parse HEAD) && \
	 test "$$actual" = "$$expected" || \
	 { echo 'fmt checkout does not match FMT_REF; supply a clean pinned checkout.' >&2; exit 1; }
	@test -z "$$(git -C "$(FMT_DIR)" status --porcelain)" || \
	 { echo 'fmt checkout has local changes; preserve it and supply a clean pinned checkout.' >&2; exit 1; }
$(FMT_OBJ): | verify-fmt

# Explicit focused-suite registrations travel with their implementation.
include mk/portable-tests.mk
