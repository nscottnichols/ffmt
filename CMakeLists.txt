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
