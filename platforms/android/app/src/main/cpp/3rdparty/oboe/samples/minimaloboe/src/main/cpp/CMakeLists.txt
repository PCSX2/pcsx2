#
# Copyright 2022 The Android Open Source Project
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

# Set the path to the Oboe library directory
set (OBOE_DIR ../../../../../)
#message("OBOE_DIR = " + ${OBOE_DIR})

add_subdirectory(${OBOE_DIR} ./oboe-bin)

# include folders
include_directories(
        ${OBOE_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}
)

# App specific sources
set (APP_SOURCES
        SimpleNoiseMaker.cpp
        MinimalOboeJNI.cpp
        )

# Build the minimaloboe (native) library
add_library(minimaloboe SHARED
        ${APP_SOURCES}
        )

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable after done debugging.
target_compile_options(minimaloboe PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-O3 -ffast-math>")

target_link_libraries( # Specifies the target library.
        minimaloboe
        oboe

        # Links the target library to the log library
        # included in the NDK.
        log)
target_link_options(minimaloboe PRIVATE "-Wl,-z,max-page-size=16384")
