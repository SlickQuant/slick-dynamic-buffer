find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# slick-stream-buffer: tests use it as a backend
find_package(slick-stream-buffer 1.0.5 CONFIG QUIET)
if(NOT slick-stream-buffer_FOUND)
  include(FetchContent)
  set(BUILD_SLICK_STREAM_BUFFER_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    slick-stream-buffer
    GIT_REPOSITORY https://github.com/SlickQuant/slick-stream-buffer.git
    GIT_TAG v1.0.5  # See https://github.com/SlickQuant/slick-stream-buffer/releases for latest version
  )
  FetchContent_MakeAvailable(slick-stream-buffer)
else()
  message(STATUS "Found slick-stream-buffer: ${slick-stream-buffer_VERSION}")
endif()

find_package(slick-stream-buffer-multiplexer 1.0.1 CONFIG QUIET)
if(NOT slick-stream-buffer-multiplexer_FOUND)
  include(FetchContent)
  set(BUILD_SLICK_STREAM_BUFFER_MULTIPLEXER_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    slick-stream-buffer-multiplexer
    GIT_REPOSITORY https://github.com/SlickQuant/slick-stream-buffer-multiplexer.git
    GIT_TAG v1.0.1  # See https://github.com/SlickQuant/slick-stream-buffer-multiplexer/releases for latest version
  )
  FetchContent_MakeAvailable(slick-stream-buffer-multiplexer)
else()
  message(STATUS "Found slick-stream-buffer-multiplexer: ${slick-stream-buffer-multiplexer_VERSION}")
endif()

add_executable(slick-dynamic-buffer-tests
  dynamic_buffer_tests.cpp
)

target_link_libraries(slick-dynamic-buffer-tests PRIVATE
  slick::dynamic_buffer
  slick::stream_buffer
  slick::stream_buffer_multiplexer
  ${SLICK_DYNAMIC_BUFFER_BOOST_TARGET}
  GTest::gtest_main)

target_compile_definitions(slick-dynamic-buffer-tests PRIVATE
  SLICK_STREAM_BUFFER_ENABLE_LOSS_DETECTION=1
  SLICK_STREAM_BUFFER_MULTIPLEXER_ENABLE_LOSS_DETECTION=1
  $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00>)

include(GoogleTest)
gtest_discover_tests(slick-dynamic-buffer-tests)

# Packaging regression test: install the project into a scratch prefix and build a
# consumer that asks for C++17. It fails if the exported target stops propagating
# cxx_std_20, or if the generated package config cannot resolve Boost on its own.
# It drives a second CMake project, so it costs far more than a unit test: skip it with
# ctest -LE packaging, or turn it off entirely. Off by default in nested builds.
option(BUILD_SLICK_DYNAMIC_BUFFER_INSTALL_TEST
       "Build the install-and-consume packaging test" ${PROJECT_IS_TOP_LEVEL})

if(BUILD_SLICK_DYNAMIC_BUFFER_INSTALL_TEST)
  add_test(NAME InstallTests.InstalledPackageBuildsWithCxx17Consumer
    COMMAND ${CMAKE_COMMAND}
      "-DPARENT_BUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DCONSUMER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/install_consumer"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install-test"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DPREFIX_PATH=${CMAKE_PREFIX_PATH}"
      "-DCTEST_EXECUTABLE=${CMAKE_CTEST_COMMAND}"
      "-DBUILD_CONFIG=$<CONFIG>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/install_and_consume.cmake")
  set_tests_properties(InstallTests.InstalledPackageBuildsWithCxx17Consumer PROPERTIES
    TIMEOUT 900
    LABELS packaging
    RUN_SERIAL TRUE)   # drives its own compiler; do not overlap with the unit tests
endif()
