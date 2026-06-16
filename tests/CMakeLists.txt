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
find_package(slick-stream-buffer 1.0.4 CONFIG QUIET)
if(NOT slick-stream-buffer_FOUND)
  include(FetchContent)
  set(BUILD_SLICK_STREAM_BUFFER_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    slick-stream-buffer
    GIT_REPOSITORY https://github.com/SlickQuant/slick-stream-buffer.git
    GIT_TAG v1.0.4  # See https://github.com/SlickQuant/slick-stream-buffer/releases for latest version
  )
  FetchContent_MakeAvailable(slick-stream-buffer)
else()
  message(STATUS "Found slick-stream-buffer: ${slick-stream-buffer_VERSION}")
endif()

find_package(slick-stream-buffer-multiplexer CONFIG QUIET)
if(NOT slick-stream-buffer-multiplexer_FOUND)
  include(FetchContent)
  set(BUILD_SLICK_STREAM_BUFFER_MULTIPLEXER_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    slick-stream-buffer-multiplexer
    GIT_REPOSITORY https://github.com/SlickQuant/slick-stream-buffer-multiplexer.git
    GIT_TAG v1.0.0  # See https://github.com/SlickQuant/slick-stream-buffer-multiplexer/releases for latest version
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
  Boost::asio
  GTest::gtest_main)

target_compile_definitions(slick-dynamic-buffer-tests PRIVATE
  SLICK_STREAM_BUFFER_ENABLE_LOSS_DETECTION=1
  SLICK_STREAM_BUFFER_MULTIPLEXER_ENABLE_LOSS_DETECTION=1
  _WIN32_WINNT=0x0A00)

# if(MSVC)
#   target_compile_options(slick-dynamic-buffer-tests PRIVATE /wd4996)
# endif()

include(GoogleTest)
gtest_discover_tests(slick-dynamic-buffer-tests)
