# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.22.1)

#PROJECT(wavlib C CXX)

#message("CMAKE_CURRENT_LIST_DIR = " ${CMAKE_CURRENT_LIST_DIR})

#message("HOME is " ${HOME})

# SET(NDK "")
#message("NDK is " ${NDK})

# Set the path to the Oboe library directory
set (OBOE_DIR ../../../../../)
#message("OBOE_DIR = " + ${OBOE_DIR})

# Pull in parselib
set (PARSELIB_DIR ../../../../parselib)
#message("PARSELIB_DIR = " + ${PARSELIB_DIR})

# compiler flags
# -mhard-float -D_NDK_MATH_NO_SOFTFP=1
#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -mhard-float -D_NDK_MATH_NO_SOFTFP=1" )

# include folders
include_directories(
        ${PARSELIB_DIR}/src/main/cpp
        ${OBOE_DIR}/include
        ${OBOE_DIR}/src/flowgraph
        ${CMAKE_CURRENT_LIST_DIR}
        ../../../../shared)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        iolib

        # Sets the library as a static library.
        STATIC

        # source
        ${CMAKE_CURRENT_LIST_DIR}/player/SampleSource.cpp
        ${CMAKE_CURRENT_LIST_DIR}/player/SampleBuffer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/player/OneShotSampleSource.cpp
        ${CMAKE_CURRENT_LIST_DIR}/player/SimpleMultiPlayer.cpp)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
            iolib

            # Links the target library to the log library
            # included in the NDK.
            log)
