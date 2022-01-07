cmake_minimum_required(VERSION 3.14)
project(replication_test)

set(CMAKE_C_STANDARD 99)
# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 11)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

#include_directories(..)

enable_testing()

add_executable(
  replication_test_run
  replication_tests.cpp
)

#target_link_libraries(replication_test_run function_libs_to_be_tested)

target_link_libraries(
  replication_test_run
  gtest_main
)

include(GoogleTest)
gtest_discover_tests(replication_test_run)