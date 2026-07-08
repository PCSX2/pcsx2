cmake_minimum_required(VERSION 3.22.1)

# Usually this file is called from run_tests.sh which requires the $ANDROID_NDK variable to be set so there's no need
# to set it here. Comments below are left in intentionally in case they're useful in future.
# This may work on Mac OS.
# set(ANDROID_NDK $ENV{HOME}/Library/Android/sdk/ndk-bundle)
# This may work on Linux.
# set(ANDROID_NDK $ENV{HOME}/Android/sdk/ndk-bundle)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -std=c++17 -DOBOE_SUPPRESS_LOG_SPAM")

# Include GoogleTest library
set(GOOGLETEST_ROOT ${ANDROID_NDK}/sources/third_party/googletest)
add_library(gtest STATIC ${GOOGLETEST_ROOT}/src/gtest_main.cc ${GOOGLETEST_ROOT}/src/gtest-all.cc)
target_include_directories(gtest PRIVATE ${GOOGLETEST_ROOT})
target_include_directories(gtest PUBLIC ${GOOGLETEST_ROOT}/include)

# Include Oboe sources
set (OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${OBOE_DIR} ./oboe-bin)
include_directories(
		${OBOE_DIR}/include
		${OBOE_DIR}/src
		)

# Build the test binary
add_executable(
		testOboe
		testAAudio.cpp
		testAudioClock.cpp
		testFlowgraph.cpp
		testFullDuplexStream.cpp
		testResampler.cpp
		testReturnStop.cpp
		testReturnStopDeadlock.cpp
		testStreamClosedMethods.cpp
		testStreamFramesProcessed.cpp
		testStreamOpen.cpp
		testStreamStates.cpp
		testStreamStop.cpp
		testStreamWaitState.cpp
		testXRunBehaviour.cpp
		testUtilities.cpp
        )

target_link_libraries(testOboe gtest oboe)
target_link_options(testOboe PRIVATE "-Wl,-z,max-page-size=16384")
