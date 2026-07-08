cmake_minimum_required(VERSION 3.22.1)

set(GOOGLETEST_ROOT ${ANDROID_NDK}/sources/third_party/googletest/googletest)
add_library(gtest STATIC ${GOOGLETEST_ROOT}/src/gtest_main.cc ${GOOGLETEST_ROOT}/src/gtest-all.cc)
target_include_directories(gtest PRIVATE ${GOOGLETEST_ROOT})
target_include_directories(gtest PUBLIC ${GOOGLETEST_ROOT}/include)

include_directories(../src/main/cpp/)

# Build our test binary
add_executable (testRhythmGame testLockFreeQueue.cpp)
target_link_libraries(testRhythmGame  gtest)
