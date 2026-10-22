cmake_minimum_required(VERSION 3.19)
project(ffmt LANGUAGES CXX)

add_library(ffmt INTERFACE)
add_library(ffmt::ffmt ALIAS ffmt)
target_include_directories(ffmt INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(ffmt INTERFACE cxx_std_20)

option(FFMT_BUILD_TESTS "Enable independent conformance tests" OFF)
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
endif()

include(GNUInstallDirs)
install(TARGETS ffmt EXPORT ffmtTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ffmtTargets NAMESPACE ffmt:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffmt)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ffmtConfig.cmake"
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/ffmtTargets.cmake\")\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ffmtConfig.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffmt)
