# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.22.1)

#PROJECT(wavlib C CXX)

#message("CMAKE_CURRENT_LIST_DIR = " ${CMAKE_CURRENT_LIST_DIR})

#message("HOME is " ${HOME})

# SET(NDK "")
#message("NDK is " ${NDK})

# compiler flags
# -mhard-float -D_NDK_MATH_NO_SOFTFP=1
#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -mhard-float -D_NDK_MATH_NO_SOFTFP=1" )

# Set the path to the Oboe library directory
set (OBOE_DIR ../../../../../)
message("OBOE_DIR = " + ${OBOE_DIR})

#add_subdirectory(${OBOE_DIR} ./oboe-bin)

# include folders
include_directories(
        ${OBOE_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}
        ../../../../shared)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        parselib

        # Sets the library as a static library.
        STATIC

        # Provides a relative path to your source file(s).
        # stream
        ${CMAKE_CURRENT_LIST_DIR}/stream/FileInputStream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/stream/InputStream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/stream/MemInputStream.cpp
        # wav
        ${CMAKE_CURRENT_LIST_DIR}/wav/AudioEncoding.cpp
        ${CMAKE_CURRENT_LIST_DIR}/wav/WavChunkHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/wav/WavFmtChunkHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/wav/WavRIFFChunkHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/wav/WavStreamReader.cpp)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
            parselib

            # Links the target library to the log library
            # included in the NDK.
            log)
