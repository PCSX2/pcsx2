# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=arm64
export CI_TARGET_SYSTEM=darwin

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=Darwin
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
    -DCMAKE_OSX_ARCHITECTURES=$CI_TARGET_ARCH
"
