cmake_minimum_required(VERSION 3.10)

project(slick-dynamic-buffer
        VERSION 1.0.1
        DESCRIPTION "Header-only Boost.Asio DynamicBuffer adapter for slick buffer backends (stream_buffer, producer_buffer) - zero-copy fan-out of received network bytes to lock-free consumers"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Options:
option(BUILD_SLICK_DYNAMIC_BUFFER_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_library(slick-dynamic-buffer INTERFACE)
add_library(slick::dynamic_buffer ALIAS slick-dynamic-buffer)

target_include_directories(slick-dynamic-buffer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(slick-dynamic-buffer PROPERTIES EXPORT_NAME dynamic_buffer)

target_link_libraries(slick-dynamic-buffer INTERFACE Boost::asio)

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

# Install the config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/slick-dynamic-bufferConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/slick-dynamic-bufferConfigVersion.cmake"
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
