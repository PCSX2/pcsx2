cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -std=c++17 -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

link_directories(${CMAKE_CURRENT_LIST_DIR}/..)

# Increment this number when adding files to OboeTester => 111
# The change in this file will help Android Studio resync
# and generate new build files that reference the new code.
file(GLOB_RECURSE app_native_sources src/main/cpp/*)

### Name must match loadLibrary() call in MainActivity.java
add_library(oboetester SHARED ${app_native_sources})

### INCLUDE OBOE LIBRARY ###

# Set the path to the Oboe library directory
set (OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# Add the Oboe library as a subproject. Since Oboe is an out-of-tree source library we must also
# specify a binary directory
add_subdirectory(${OBOE_DIR} ./oboe-bin)

# Specify the path to the Oboe header files and the source.
include_directories(
    ${OBOE_DIR}/include
    ${OBOE_DIR}/src
)

### END OBOE INCLUDE SECTION ###

# link to oboe
target_link_libraries(oboetester log oboe atomic)
target_link_options(oboetester PRIVATE "-Wl,-z,max-page-size=16384")
