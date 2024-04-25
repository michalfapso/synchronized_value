cmake_minimum_required(VERSION 3.10)

# set the project name
project(synchronized_value VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# find_package(Catch2 3 REQUIRED)
Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)
FetchContent_MakeAvailable(Catch2)

# add the executable
add_executable(tests
    synchronized_value_test.cpp
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)