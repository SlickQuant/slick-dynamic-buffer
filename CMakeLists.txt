# 3.21 for PROJECT_IS_TOP_LEVEL (used below) and ctest --test-dir in the install test.
cmake_minimum_required(VERSION 3.21)

project(slick-dynamic-buffer
        VERSION 1.0.2
        DESCRIPTION "Header-only Boost.Asio DynamicBuffer adapter for slick buffer backends (stream_buffer, producer_buffer) - zero-copy fan-out of received network bytes to lock-free consumers"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Options:
option(BUILD_SLICK_DYNAMIC_BUFFER_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

# Only the header-only part of Boost.Asio is used, but which imported target provides it
# differs per distribution (see cmake/slick_find_boost_asio.cmake). The installed package
# config runs the same resolution on the consumer machine, so nothing about this build
# environment is baked into the package.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/slick_find_boost_asio.cmake")
slick_dynamic_buffer_find_boost_asio(SLICK_DYNAMIC_BUFFER_BOOST_TARGET)
if(NOT SLICK_DYNAMIC_BUFFER_BOOST_TARGET)
    message(FATAL_ERROR
        "Boost.Asio headers not found. Install Boost (>= 1.66) or point CMAKE_PREFIX_PATH "
        "/ CMAKE_TOOLCHAIN_FILE at a Boost installation, e.g. vcpkg install boost-asio.")
endif()
message(STATUS "${PROJECT_NAME}: using Boost target ${SLICK_DYNAMIC_BUFFER_BOOST_TARGET}")

add_library(slick-dynamic-buffer INTERFACE)
add_library(slick::dynamic_buffer ALIAS slick-dynamic-buffer)

target_include_directories(slick-dynamic-buffer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(slick-dynamic-buffer PROPERTIES EXPORT_NAME dynamic_buffer)

# The header uses concepts: CMAKE_CXX_STANDARD above only affects this build, so state
# the requirement on the target itself or consumers may compile it as C++17.
target_compile_features(slick-dynamic-buffer INTERFACE cxx_std_20)

# Build tree only. The installed target must not name a Boost target chosen here: the
# consumer may have Boost::headers where this machine had Boost::asio. The package config
# resolves Boost itself and appends the result to the imported target.
target_link_libraries(slick-dynamic-buffer INTERFACE
    $<BUILD_INTERFACE:${SLICK_DYNAMIC_BUFFER_BOOST_TARGET}>)

if(BUILD_SLICK_DYNAMIC_BUFFER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Generate and install CMake config files for vcpkg compatibility
include(CMakePackageConfigHelpers)

# Create the config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slick-dynamic-bufferConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slick-dynamic-bufferConfig.cmake"
    INSTALL_DESTINATION lib/cmake/slick-dynamic-buffer
)

# Create the version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/slick-dynamic-bufferConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config files, including the Boost resolution the config shares with this
# build - the consumer runs it against its own Boost installation.
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/slick-dynamic-bufferConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/slick-dynamic-bufferConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slick_find_boost_asio.cmake"
    DESTINATION lib/cmake/slick-dynamic-buffer
)

# Install the target
install(TARGETS slick-dynamic-buffer
    EXPORT slick-dynamic-bufferTargets
)

# Install the export set
install(EXPORT slick-dynamic-bufferTargets
    FILE slick-dynamic-bufferTargets.cmake
    NAMESPACE slick::
    DESTINATION lib/cmake/slick-dynamic-buffer
)

message(STATUS "${PROJECT_NAME}: ${PROJECT_VERSION}")
