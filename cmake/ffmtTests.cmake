# The original shared host/device cases and direct API assertions have one
# explicit source inventory. CMake only supplies a second build frontend.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Git REQUIRED)
set(FFMT_FMT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt" CACHE PATH
  "Clean fmt checkout at the immutable reference revision")
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
  message(FATAL_ERROR "The fmt reference checkout is modified or cannot be inspected. Preserve edits and supply a clean pinned FFMT_FMT_DIR. ${ffmt_fmt_status_error}")
endif()

# Parse the intentionally small Make inventory: simple name := value lines.
# No discovery glob or duplicate list decides which tests exist.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/test/direct/tests.mk" ffmt_test_declarations)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/test/direct/tests.mk")
foreach(declaration IN LISTS ffmt_test_declarations)
  if(declaration MATCHES "^(FFMT_[A-Za-z0-9_]+)[ \t]*:=[ \t]*(.*)$")
    string(STRIP "${CMAKE_MATCH_2}" value)
    separate_arguments(value UNIX_COMMAND "${value}")
    set("${CMAKE_MATCH_1}" "${value}")
  endif()
endforeach()

# fmt's implementation is compiled once for the host, as in the Make harness.
add_library(ffmt_test_reference STATIC "${FFMT_FMT_DIR}/src/format.cc")
target_include_directories(ffmt_test_reference SYSTEM PUBLIC "${FFMT_FMT_DIR}/include")
target_compile_features(ffmt_test_reference PUBLIC cxx_std_20)
set_target_properties(ffmt_test_reference PROPERTIES CXX_EXTENSIONS OFF)

function(ffmt_test_options target)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:/fp:precise>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-fast-math>)
endfunction()
ffmt_test_options(ffmt_test_reference)

foreach(path specifiers buffer)
  set(sources test/test_main_host.cpp)
  foreach(suite IN LISTS FFMT_SHARED_TESTS FFMT_HOST_ONLY_TESTS)
    list(APPEND sources "test/test_${suite}.cpp")
  endforeach()
  if(path STREQUAL buffer)
    foreach(suite IN LISTS FFMT_BUFFER_TESTS)
      list(APPEND sources "test/test_${suite}.cpp")
    endforeach()
    set(buffer_path 1)
  else()
    set(buffer_path 0)
  endif()
  add_executable(ffmt_test_host_${path} ${sources})
  target_link_libraries(ffmt_test_host_${path} PRIVATE ffmt::ffmt ffmt_test_reference)
  target_compile_definitions(ffmt_test_host_${path} PRIVATE TEST_NO_MAIN FFMT_BUFFER_PATH=${buffer_path})
  ffmt_test_options(ffmt_test_host_${path})
  add_test(NAME ffmt.host-${path} COMMAND ffmt_test_host_${path})

  add_executable(ffmt_test_fast_math_${path} test/test_fast_math.cpp)
  target_link_libraries(ffmt_test_fast_math_${path} PRIVATE ffmt::ffmt ffmt_test_reference)
  target_compile_definitions(ffmt_test_fast_math_${path} PRIVATE FFMT_BUFFER_PATH=${buffer_path})
  ffmt_test_options(ffmt_test_fast_math_${path})
  target_compile_options(ffmt_test_fast_math_${path} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/fp:fast>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffast-math>)
  add_test(NAME ffmt.fast-math-${path} COMMAND ffmt_test_fast_math_${path})
endforeach()

set(direct_tests ${FFMT_DIRECT_TESTS})
if(FFMT_BUILD_CXX23_TESTS)
  list(APPEND direct_tests ${FFMT_DIRECT_CXX23_TESTS})
endif()
foreach(suite IN LISTS direct_tests)
  set(source "${suite}")
  if(DEFINED FFMT_DIRECT_SOURCE_${suite})
    set(source "${FFMT_DIRECT_SOURCE_${suite}}")
  endif()
  add_executable(ffmt_test_${suite} "test/direct/test_${source}.cpp")
  target_link_libraries(ffmt_test_${suite} PRIVATE ffmt::ffmt)
  if(FFMT_DIRECT_REFERENCE_${suite})
    target_link_libraries(ffmt_test_${suite} PRIVATE ffmt_test_reference)
  endif()
  ffmt_test_options(ffmt_test_${suite})
  target_compile_options(ffmt_test_${suite} PRIVATE ${FFMT_DIRECT_FLAGS_${suite}})
  if(suite IN_LIST FFMT_DIRECT_CXX23_TESTS)
    target_compile_features(ffmt_test_${suite} PRIVATE cxx_std_23)
  endif()
  add_test(NAME ffmt.${suite} COMMAND ffmt_test_${suite})
  if(FFMT_DIRECT_OPTIONAL_${suite})
    set_tests_properties(ffmt.${suite} PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endforeach()

# Isolate the implementation header from all wrappers and the reference library.
configure_file(include/ffmt/base.hpp single-header/ffmt/base.hpp COPYONLY)
add_executable(ffmt_test_single_header test/test_single_header.cpp)
target_include_directories(ffmt_test_single_header PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/single-header")
target_compile_features(ffmt_test_single_header PRIVATE cxx_std_20)
ffmt_test_options(ffmt_test_single_header)
add_test(NAME ffmt.single-header COMMAND ffmt_test_single_header)

add_test(NAME ffmt.infrastructure COMMAND "${Python3_EXECUTABLE}" -m unittest discover
  -s test -p test_infrastructure.py)
set_tests_properties(ffmt.infrastructure PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME ffmt.negative COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/negative.py" --cxx=${CMAKE_CXX_COMPILER}
  "--flags=-std=c++20 -I${CMAKE_CURRENT_SOURCE_DIR}/include -I${FFMT_FMT_DIR}/include"
  "--output=${CMAKE_CURRENT_BINARY_DIR}/negative")
set_tests_properties(ffmt.negative PROPERTIES TIMEOUT 1800)
