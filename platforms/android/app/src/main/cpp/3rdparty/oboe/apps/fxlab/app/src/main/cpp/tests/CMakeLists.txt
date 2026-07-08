cmake_minimum_required(VERSION 3.22.1)

project(Effects_Tests)
# We need C++17 to test
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Link runTests with what we want to test and the GTest and pthread library
add_executable(runTests testEffects.cpp)
target_link_libraries(runTests ${GTEST_LIBRARIES} pthread)
