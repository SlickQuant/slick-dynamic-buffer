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

add_executable(slick-dynamic-buffer-tests
  dynamic_buffer_tests.cpp
)
if(MSVC)
  add_compile_options(/wd4996)
endif()

target_link_libraries(slick-dynamic-buffer-tests PRIVATE slick::dynamic_buffer Boost::asio GTest::gtest_main)

# Enable loss detection in slick-stream-buffer dependency for testing purposes (this is not enabled by default since it adds some overhead)
target_compile_definitions(slick-dynamic-buffer-tests PRIVATE SLICK_STREAM_BUFFER_ENABLE_LOSS_DETECTION=1)

include(GoogleTest)
gtest_discover_tests(slick-dynamic-buffer-tests)
