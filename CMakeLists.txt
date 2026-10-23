cmake_minimum_required(VERSION 3.20)
project(ffmt LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(ffmt INTERFACE)
add_library(ffmt::ffmt ALIAS ffmt)
target_compile_features(ffmt INTERFACE cxx_std_20)
target_include_directories(ffmt INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Consumers install headers and an interface target; no oracle or test runtime.
install(TARGETS ffmt EXPORT ffmtTargets)
install(DIRECTORY include/ffmt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES NOTICE LICENSES/fmt.txt LICENSES/dragonbox-Boost.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ffmt/licenses)
set(ffmt_config_dir ${CMAKE_INSTALL_LIBDIR}/cmake/ffmt)
configure_package_config_file(cmake/ffmtConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ffmtConfig.cmake
  INSTALL_DESTINATION ${ffmt_config_dir})
install(EXPORT ffmtTargets NAMESPACE ffmt:: DESTINATION ${ffmt_config_dir})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ffmtConfig.cmake
  DESTINATION ${ffmt_config_dir})

option(FFMT_BUILD_TESTS "Build the portable conformance and regression tests" OFF)
option(FFMT_BUILD_CXX23_TESTS "Include optional C++23 standard-type tests" OFF)
if(FFMT_BUILD_CXX23_TESTS AND NOT FFMT_BUILD_TESTS)
  message(FATAL_ERROR "FFMT_BUILD_CXX23_TESTS requires FFMT_BUILD_TESTS")
endif()
if(FFMT_BUILD_TESTS)
  enable_testing()
  include(cmake/ffmtTests.cmake)
endif()
