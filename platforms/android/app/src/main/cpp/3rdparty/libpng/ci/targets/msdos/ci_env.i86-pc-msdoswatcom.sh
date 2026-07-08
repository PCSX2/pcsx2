# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=i86
export CI_TARGET_SYSTEM=msdoswatcom

export CI_CC="wcl"

# Open Watcom V2 CMake build
# https://github.com/open-watcom/open-watcom-v2/discussions/716
export CI_CMAKE_GENERATOR="Watcom WMake"
export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=DOS
    -DCMAKE_SYSTEM_PROCESSOR=I86
"
