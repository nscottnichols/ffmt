cmake_minimum_required(VERSION 3.19)
project(ffmt LANGUAGES CXX)

add_library(ffmt INTERFACE)
add_library(ffmt::ffmt ALIAS ffmt)
target_include_directories(ffmt INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(ffmt INTERFACE cxx_std_20)

option(FFMT_BUILD_TESTS "Enable independent conformance tests" OFF)
option(FFMT_BUILD_CXX23_TESTS "Enable optional C++23 type tests" OFF)
set(FFMT_FMT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt" CACHE PATH "Pinned host fmt oracle checkout")
if(FFMT_BUILD_TESTS)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME ffmt.harness COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${CMAKE_CURRENT_SOURCE_DIR}/test/conformance -p test_runner.py)
  add_test(NAME ffmt.host-conformance COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/conformance.py --profile local
    --route host-buffer --route host-specifiers --opt 2
    --host-cxx ${CMAKE_CXX_COMPILER} --fmt-dir ${FFMT_FMT_DIR})
  set_tests_properties(ffmt.host-conformance PROPERTIES TIMEOUT 900)
  set_tests_properties(ffmt.host-conformance PROPERTIES
    ENVIRONMENT "CXX=${CMAKE_CXX_COMPILER}")
  add_library(ffmt_test_reference STATIC "${FFMT_FMT_DIR}/src/format.cc")
  target_include_directories(ffmt_test_reference PUBLIC "${FFMT_FMT_DIR}/include")
  target_compile_features(ffmt_test_reference PUBLIC cxx_std_20)
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("#ifndef __SIZEOF_FLOAT128__\n#error unavailable\n#endif\nint main() {}"
    FFMT_TEST_HAS_FLOAT128)
  set(ffmt_test_suites engine engine_transport engine_legacy_hook engine_extended
    engine_bitint engine_quad engine_forwarding engine_fast_math families families_ranges families_printf)
  if(FFMT_BUILD_CXX23_TESTS)
    list(APPEND ffmt_test_suites families_std23)
  endif()
  foreach(suite IN LISTS ffmt_test_suites)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/conformance/test_${suite}.cpp")
      add_executable(ffmt_test_${suite} "test/conformance/test_${suite}.cpp")
      target_link_libraries(ffmt_test_${suite} PRIVATE ffmt::ffmt ffmt_test_reference)
      target_compile_options(ffmt_test_${suite} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
      if(suite STREQUAL "families_std23")
        target_compile_features(ffmt_test_${suite} PRIVATE cxx_std_23)
      endif()
      if(suite STREQUAL "engine_quad" AND FFMT_TEST_HAS_FLOAT128)
        target_compile_definitions(ffmt_test_${suite} PRIVATE FMT_USE_FLOAT128=1)
      endif()
      if(suite STREQUAL "engine_fast_math")
        target_compile_options(ffmt_test_${suite} PRIVATE
          $<$<CXX_COMPILER_ID:MSVC>:/fp:fast>
          $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffast-math>)
      endif()
      add_test(NAME ffmt.${suite} COMMAND ffmt_test_${suite})
      set_tests_properties(ffmt.${suite} PROPERTIES SKIP_RETURN_CODE 77)
    endif()
  endforeach()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/conformance/test_engine.cpp")
    add_executable(ffmt_test_engine_portable test/conformance/test_engine.cpp)
    target_link_libraries(ffmt_test_engine_portable PRIVATE ffmt::ffmt ffmt_test_reference)
    target_compile_definitions(ffmt_test_engine_portable PRIVATE FFMT_PORTABLE_SHORTEST=1)
    target_compile_options(ffmt_test_engine_portable PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME ffmt.engine_portable COMMAND ffmt_test_engine_portable)
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS ffmt EXPORT ffmtTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES NOTICE LICENSES/fmt.txt LICENSES/dragonbox-Boost.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ffmt/licenses)
install(EXPORT ffmtTargets NAMESPACE ffmt:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffmt)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ffmtConfig.cmake"
  [=[include("${CMAKE_CURRENT_LIST_DIR}/ffmtTargets.cmake")
]=])
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ffmtConfig.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffmt)
