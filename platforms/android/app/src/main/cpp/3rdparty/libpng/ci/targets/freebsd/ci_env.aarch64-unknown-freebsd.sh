# Copyright (c) 2023-2024 Cosmin Truta.
#
# Use, modification and distribution are subject to the MIT License.
# Please see the accompanying file LICENSE_MIT.txt
#
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=aarch64
export CI_TARGET_SYSTEM=freebsd

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=FreeBSD
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
