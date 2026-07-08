# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)

# Require C++17 compiler with no extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

link_directories(${CMAKE_CURRENT_LIST_DIR}/..)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        native-lib
        # Sets the library as a shared library.
        SHARED
        # Provides a relative path to your source file(s).
        src/main/cpp/native-lib.cpp
        src/main/cpp/DuplexEngine.cpp
        )

# Compile with warnings for safety
if(MSVC)
    target_compile_options(native-lib PRIVATE /W4 /WX)
else()
    target_compile_options(native-lib PRIVATE -Wall -Wextra -pedantic)
endif()

target_compile_options(native-lib PRIVATE -Ofast)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib
        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)


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


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        native-lib
        oboe
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})
target_link_options(native-lib PRIVATE "-Wl,-z,max-page-size=16384")
