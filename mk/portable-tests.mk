# Registrations explicitly name every required source; source globs never decide
# which declared tests exist. Independent capabilities add independent .mk files.
FFMT_CAP_BUILD_RULES := $(lastword $(MAKEFILE_LIST))
FFMT_CAPABILITIES :=
include $(sort $(wildcard test/conformance/suites/*.mk))
FFMT_CAP_REGISTRATIONS := $(FFMT_CAP_BUILD_RULES) $(sort $(wildcard test/conformance/suites/*.mk))
FFMT_CAP_HEADERS := $(wildcard include/ffmt/*.hpp include/ffmt/detail/*.hpp)

define FFMT_CAPABILITY
build/cap_$(1): $$(FFMT_SOURCE_$(1)) $$(FFMT_CAP_HEADERS) $$(FFMT_CAP_REGISTRATIONS) $$(FLAGS_STAMP) $(if $(filter 1,$(FFMT_REFERENCE_$(1))),$(FMT_OBJ)) | build/.dir
	$$(CXX) $$(CXXFLAGS) $$(HOST_OPT) -std=$$(FFMT_STANDARD_$(1)) $$(FFMT_FLAGS_$(1)) -UNDEBUG $$< $(if $(filter 1,$(FFMT_REFERENCE_$(1))),$(FMT_OBJ)) -o $$@
build/focused_$(1): build/cap_$(1)
	ln -sf $$(notdir $$<) $$@
endef
$(foreach suite,$(FFMT_CAPABILITIES),$(eval $(call FFMT_CAPABILITY,$(suite))))

FFMT_CXX23_CAPABILITIES := $(foreach suite,$(FFMT_CAPABILITIES),$(if $(filter c++23,$(FFMT_STANDARD_$(suite))),$(suite)))
FFMT_DEFAULT_CAPABILITIES := $(filter-out $(FFMT_CXX23_CAPABILITIES),$(FFMT_CAPABILITIES))
FFMT_CAP_BINS := $(addprefix build/cap_,$(FFMT_DEFAULT_CAPABILITIES))
# Optional compiler types use exit 77; this is visible as SKIP, not conformance.
# The qualification runner records each individual exit status separately.
define ffmt_run_capabilities
@failed=0; for entry in $(foreach binary,$(1),$(binary):$(or $(FFMT_OPTIONAL_$(patsubst build/cap_%,%,$(binary))),0)); do \
  binary=$${entry%:*}; optional=$${entry##*:}; \
  ./$$binary; result=$$?; \
  if [ $$result -eq 0 ]; then echo "$$binary: PASS"; \
  elif [ $$result -eq 77 ] && [ $$optional -eq 1 ]; then echo "$$binary: SKIP (optional compiler capability)"; \
  else echo "$$binary: FAIL ($$result)"; failed=1; fi; \
done; exit $$failed
endef

.PHONY: test-capabilities test-focused conformance-harness list-capabilities
# Existing users can retain the focused-suite target name.
test-focused: test-capabilities
test-capabilities: $(FFMT_CAP_BINS)
	$(call ffmt_run_capabilities,$^)

list-capabilities:
	@$(foreach suite,$(FFMT_CAPABILITIES),echo '$(suite) $(FFMT_SOURCE_$(suite))';)

conformance-harness:
	python3 -m unittest discover -s test/conformance -p 'test_*.py'

# The C++23 implementation registers its explicit sources in the same registry.
.PHONY: test-cxx23
test-cxx23: $(addprefix build/cap_,$(FFMT_CXX23_CAPABILITIES))
	$(call ffmt_run_capabilities,$^)
