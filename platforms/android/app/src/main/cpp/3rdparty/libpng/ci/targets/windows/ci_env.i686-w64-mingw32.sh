# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=i686
export CI_TARGET_SYSTEM=mingw32

# The output of `uname -s` on MSYS2 is understandable, and so is
# CI_TARGET_SYSTEM above, in simplified form. (See also Cygwin.)
# But aside from that, the Mingw-w64 nomenclature is rather messy.
export CI_CC="$CI_TARGET_ARCH-w64-mingw32-gcc"
export CI_AR="$CI_CC-ar"
export CI_RANLIB="$CI_CC-ranlib"

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=Windows
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
