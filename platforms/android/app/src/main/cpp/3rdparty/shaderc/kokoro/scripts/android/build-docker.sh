#!/bin/bash

# Copyright (C) 2017-2022 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Android Build Script.


# Fail on any error.
set -e

. /bin/using.sh # Declare the bash `using` function for configuring toolchains.

# Display commands being run.
set -x

using cmake-3.31.2
using ninja-1.10.0
using ndk-r27c # Sets ANDROID_NDK_HOME, pointing at the NDK's root dir

git config --global --add safe.directory '*'

cd $ROOT_DIR
./utils/git-sync-deps

[ -d build ] || mkdir build
cd $ROOT_DIR/build

# Invoke the build.
BUILD_SHA=${KOKORO_GITHUB_COMMIT:-$KOKORO_GITHUB_PULL_REQUEST_COMMIT}
echo $(date): Starting build...
cmake \
  -GNinja \
  -DCMAKE_MAKE_PROGRAM=ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_ABI="$TARGET_ARCH" \
  -DSHADERC_SKIP_TESTS=ON \
  -DSPIRV_SKIP_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_NDK=$ANDROID_NDK_HOME ..

echo $(date): Build glslang library...
ninja glslang

echo $(date): Build everything...
ninja

echo $(date): Check Shaderc for copyright notices...
ninja check-copyright

echo $(date): Build completed.
