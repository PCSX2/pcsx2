#
# Copyright 2025 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.22.1)

# Pull in required libs
set(PARSELIB_DIR ../../../../parselib)
set(IOLIB_DIR ../../../../iolib)
set(OBOE_DIR ../../../../../)

add_subdirectory(${OBOE_DIR} ./oboe-bin)

# include folders
include_directories(
        ${OBOE_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}
)

include(${PARSELIB_DIR}/src/main/cpp/CMakeLists.txt)
include(${IOLIB_DIR}/src/main/cpp/CMakeLists.txt)

# App specific sources
set(APP_SOURCES
        PowerPlayJNI.cpp
        PowerPlayMultiPlayer.h
        PowerPlayMultiPlayer.cpp
)

# Build the powerplay (native) library
add_library(
        powerplay SHARED ${APP_SOURCES}
)

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable after done debugging.
target_compile_options(powerplay PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-O3 -ffast-math>")

target_link_libraries(
        # Specifies the target library.
        powerplay

        # iolib & parselib
        -Wl,--whole-archive
        iolib
        parselib
        -Wl,--no-whole-archive

        oboe

        # Links the target library to the log library
        # included in the NDK.
        log
)

target_link_options(powerplay PRIVATE "-Wl,-z,max-page-size=16384")
