cmake_minimum_required(VERSION 3.21)

project(slick-dynamic-buffer-consumer LANGUAGES CXX)

# Deliberately C++17, and deliberately not REQUIRED: the imported target has to raise
# this to C++20 through target_compile_features, otherwise the concepts in
# <slick/dynamic_buffer.hpp> will not compile. Do not "fix" a failure here by bumping
# the standard - that would hide the packaging bug this test exists to catch.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

find_package(slick-dynamic-buffer CONFIG REQUIRED)
message(STATUS "consumer: slick-dynamic-buffer ${slick-dynamic-buffer_VERSION} from ${slick-dynamic-buffer_DIR}")

# Guard against a copy installed elsewhere (vcpkg, system prefix) answering instead of
# the freshly installed one - that would silently test the wrong package.
if(EXPECTED_PACKAGE_PREFIX)
  get_filename_component(_expected "${EXPECTED_PACKAGE_PREFIX}" REALPATH)
  get_filename_component(_actual "${slick-dynamic-buffer_DIR}/../../.." REALPATH)
  if(NOT _expected STREQUAL _actual)
    message(FATAL_ERROR "expected the package from ${_expected}, got ${_actual}")
  endif()
endif()

# Boost is header-only here, so the package must not be bound to the Boost provider of
# the machine that built it: the exported target may not name a Boost target, and the
# package config has to resolve one on this machine and attach it. Without both halves,
# a package built with vcpkg (Boost::asio) breaks for a consumer whose Boost only
# provides Boost::headers.
file(READ "${slick-dynamic-buffer_DIR}/slick-dynamic-bufferTargets.cmake" _exported_targets)
if(_exported_targets MATCHES "Boost::")
  message(FATAL_ERROR
    "the installed export names a Boost target, so the package only works where the "
    "same Boost provider exists")
endif()

get_target_property(_boost_link slick::dynamic_buffer INTERFACE_LINK_LIBRARIES)
if(NOT "${_boost_link}" MATCHES "Boost::")
  message(FATAL_ERROR "the package config attached no Boost target: ${_boost_link}")
endif()
message(STATUS "consumer: Boost resolved by the package config as ${_boost_link}")

if(EXPECTED_BOOST_TARGET AND NOT "${_boost_link}" MATCHES "${EXPECTED_BOOST_TARGET}")
  message(FATAL_ERROR "expected Boost target ${EXPECTED_BOOST_TARGET}, got ${_boost_link}")
endif()

add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE slick::dynamic_buffer)

enable_testing()
add_test(NAME consumer COMMAND consumer)
