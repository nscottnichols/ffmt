# Test dependencies are discovered only inside FFMT_BUILD_TESTS.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Git REQUIRED)
set(FFMT_FMT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt" CACHE PATH
  "Clean fmt checkout at the immutable conformance revision")
set(ffmt_fmt_revision "0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${FFMT_FMT_DIR}" rev-parse HEAD
  RESULT_VARIABLE ffmt_fmt_git_result OUTPUT_VARIABLE ffmt_fmt_actual
  ERROR_VARIABLE ffmt_fmt_git_error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ffmt_fmt_git_result EQUAL 0 OR NOT ffmt_fmt_actual STREQUAL ffmt_fmt_revision)
  message(FATAL_ERROR "FFMT_FMT_DIR must be a clean checkout of ${ffmt_fmt_revision}. Preserve local checkouts and select an existing pinned one. ${ffmt_fmt_git_error}")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${FFMT_FMT_DIR}" status --porcelain
  RESULT_VARIABLE ffmt_fmt_status_result OUTPUT_VARIABLE ffmt_fmt_status
  ERROR_VARIABLE ffmt_fmt_status_error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ffmt_fmt_status_result EQUAL 0 OR NOT ffmt_fmt_status STREQUAL "")
  message(FATAL_ERROR "The fmt oracle checkout is modified or cannot be inspected. Preserve edits and supply a clean pinned FFMT_FMT_DIR. ${ffmt_fmt_status_error}")
endif()

add_library(ffmt_test_reference STATIC "${FFMT_FMT_DIR}/src/format.cc")
target_include_directories(ffmt_test_reference PUBLIC "${FFMT_FMT_DIR}/include")
target_compile_features(ffmt_test_reference PUBLIC cxx_std_20)
set_target_properties(ffmt_test_reference PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(ffmt_test_reference PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/fp:precise>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-fast-math>)

function(ffmt_add_regression suite source)
  # An absent declared source is a configuration error, not a skipped test.
  add_executable(ffmt_test_${suite} "test/conformance/${source}")
  target_link_libraries(ffmt_test_${suite} PRIVATE ffmt::ffmt ffmt_test_reference)
  set_target_properties(ffmt_test_${suite} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_options(ffmt_test_${suite} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
  add_test(NAME ffmt.${suite} COMMAND ffmt_test_${suite})
endfunction()

# Core behavior and optional families each retain a visible test entry.
ffmt_add_regression(core_accounting test_core_accounting.cpp)
ffmt_add_regression(core_parsing test_core_parsing.cpp)
ffmt_add_regression(core_primitives test_core_primitives.cpp)
ffmt_add_regression(core_conversion test_core_conversion.cpp)
ffmt_add_regression(core_floating test_core_floating.cpp)
ffmt_add_regression(core_int128 test_core_int128.cpp)
ffmt_add_regression(engine_bitint test_engine_bitint.cpp)
ffmt_add_regression(engine_extended test_engine_extended.cpp)
ffmt_add_regression(engine_quad test_engine_quad.cpp)
ffmt_add_regression(core_unicode test_core_unicode.cpp)
ffmt_add_regression(core_formatters test_core_formatters.cpp)
ffmt_add_regression(core_forwarding test_core_forwarding.cpp)
ffmt_add_regression(formatter_composition test_formatter_composition.cpp)
ffmt_add_regression(core_format_as test_core_format_as.cpp)
ffmt_add_regression(core_legacy test_core_legacy.cpp)
ffmt_add_regression(legacy_forwarding test_legacy_forwarding.cpp)
ffmt_add_regression(engine_transport test_engine_transport.cpp)
ffmt_add_regression(engine_legacy_hook test_engine_legacy_hook.cpp)
ffmt_add_regression(transport_forwarding test_transport_forwarding.cpp)
ffmt_add_regression(core_migration test_core_migration.cpp)
ffmt_add_regression(engine_fast_math test_engine_fast_math.cpp)
ffmt_add_regression(named_arguments test_named_arguments.cpp)
ffmt_add_regression(family_support test_family_support.cpp)
ffmt_add_regression(ranges test_ranges.cpp)
ffmt_add_regression(ranges_iterators test_ranges_iterators.cpp)
ffmt_add_regression(ranges_forwarding test_ranges_forwarding.cpp)
ffmt_add_regression(chrono test_chrono.cpp)
ffmt_add_regression(std test_std.cpp)
ffmt_add_regression(style test_style.cpp)
ffmt_add_regression(enum test_enum.cpp)
ffmt_add_regression(printf test_printf.cpp)
ffmt_add_regression(feature_integration test_feature_integration.cpp)

# Exercise the full seeded floating suite through the integer interval path.
ffmt_add_regression(core_floating_portable test_core_floating.cpp)
target_compile_definitions(ffmt_test_core_floating_portable PRIVATE FFMT_PORTABLE_SHORTEST=1)

target_compile_options(ffmt_test_engine_fast_math PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/fp:fast>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffast-math>)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#ifndef __SIZEOF_FLOAT128__\n#error unavailable\n#endif\nint main() { __float128 value = 0; return value != 0; }"
  FFMT_TEST_HAS_FLOAT128)
if(FFMT_TEST_HAS_FLOAT128)
  target_compile_definitions(ffmt_test_engine_quad PRIVATE FMT_USE_FLOAT128=1)
endif()
foreach(suite core_int128 engine_bitint engine_quad)
  set_tests_properties(ffmt.${suite} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
if(FFMT_BUILD_CXX23_TESTS)
  ffmt_add_regression(std23 test_std23.cpp)
  target_compile_features(ffmt_test_std23 PRIVATE cxx_std_23)
  set_tests_properties(ffmt.std23 PROPERTIES SKIP_RETURN_CODE 77)
endif()

# New capability tests participate automatically; helper modules do not match.
add_test(NAME ffmt.harness COMMAND "${Python3_EXECUTABLE}" -m unittest discover
  -s "${CMAKE_CURRENT_SOURCE_DIR}/test/conformance" -p "test_*.py")
set_tests_properties(ffmt.harness PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME ffmt.host-conformance COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/conformance.py"
  --route host-buffer --route host-specifiers --opt 2
  --host-cxx "${CMAKE_CXX_COMPILER}" --fmt-dir "${FFMT_FMT_DIR}")
set_tests_properties(ffmt.host-conformance PROPERTIES
  TIMEOUT 1800 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  ENVIRONMENT "CXX=${CMAKE_CXX_COMPILER}")
