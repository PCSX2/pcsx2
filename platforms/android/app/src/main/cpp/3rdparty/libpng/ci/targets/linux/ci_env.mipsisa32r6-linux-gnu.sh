# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=mipsisa32r6
export CI_TARGET_SYSTEM=linux
export CI_TARGET_ABI=gnu

export CI_GCC="${CI_GCC-gcc}"

export CI_CC="$CI_TARGET_ARCH-$CI_TARGET_SYSTEM-$CI_TARGET_ABI-$CI_GCC"
export CI_AR="$CI_TARGET_ARCH-$CI_TARGET_SYSTEM-$CI_TARGET_ABI-ar"
export CI_RANLIB="$CI_TARGET_ARCH-$CI_TARGET_SYSTEM-$CI_TARGET_ABI-ranlib"

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=Linux
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
