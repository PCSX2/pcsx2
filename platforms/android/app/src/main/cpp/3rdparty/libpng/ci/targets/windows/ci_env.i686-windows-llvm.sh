# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=i686
export CI_TARGET_SYSTEM=windows

export CI_CC="clang"
export CI_AR="llvm-ar"
export CI_RANLIB="llvm-ranlib"

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=Windows
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
