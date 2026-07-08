# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=i686
export CI_TARGET_SYSTEM=cygwin

export CI_CC="$CI_TARGET_ARCH-pc-$CI_TARGET_SYSTEM-gcc"
export CI_AR="$CI_CC-ar"
export CI_RANLIB="$CI_CC-ranlib"

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=CYGWIN
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
