# Collect the mandatory registered runtime checks as well as the legacy paths.
# Optional extensions and fast-math modes have separate qualification results.
FFMT_COV_SUITES := $(foreach suite,$(FFMT_CAPABILITIES),$(if $(filter 1,$(FFMT_OPTIONAL_$(suite))),,$(if $(findstring fast_math,$(suite)),,$(suite))))

define FFMT_CAPABILITY_COVERAGE
build/cov_cap_$(1): $$(FFMT_SOURCE_$(1)) $$(FFMT_CAP_HEADERS) $$(FFMT_CAP_REGISTRATIONS) $$(FLAGS_STAMP) $(if $(filter 1,$(FFMT_REFERENCE_$(1))),$(FMT_OBJ)) | build/.dir
	$$(CXX) $$(CXXFLAGS) $$(HOST_OPT) -std=$$(FFMT_STANDARD_$(1)) $$(FFMT_FLAGS_$(1)) $$(COV_FLAGS) -UNDEBUG $$< $(if $(filter 1,$(FFMT_REFERENCE_$(1))),$(FMT_OBJ)) -o $$@
endef
$(foreach suite,$(FFMT_COV_SUITES),$(eval $(call FFMT_CAPABILITY_COVERAGE,$(suite))))
coverage: $(addprefix build/cov_cap_,$(FFMT_COV_SUITES))
