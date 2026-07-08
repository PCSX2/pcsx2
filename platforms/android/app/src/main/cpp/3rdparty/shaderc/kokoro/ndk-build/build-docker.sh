#!/bin/bash
# Copyright (c) 2018 Google LLC.
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
# Linux Build Script.

# Fail on any error.
set -e
# Display commands being run.
set -x

# This is required to run any git command in the docker since owner will
# have changed between the clone environment, and the docker container.
# Marking the root of the repo as safe for ownership changes.
git config --global --add safe.directory $ROOT_DIR

. /bin/using.sh # Declare the bash `using` function for configuring toolchains.

cd $ROOT_DIR

function clean_dir() {
  dir=$1
  if [[ -d "$dir" ]]; then
    rm -fr "$dir"
  fi
  mkdir "$dir"
}

# Get source for dependencies, as specified in the DEPS file
/usr/bin/python3 utils/git-sync-deps --treeless

using ndk-r27c

clean_dir "$ROOT_DIR/build"
cd "$ROOT_DIR/build"

function do_ndk_build () {
  echo $(date): Starting ndk-build $@...
  $ANDROID_NDK_HOME/ndk-build \
    -C $ROOT_DIR/android_test \
    NDK_PROJECT_PATH=. \
    NDK_LIBS_OUT=./libs \
    NDK_APP_OUT=./app \
    V=1 \
    SPVTOOLS_LOCAL_PATH=$ROOT_DIR/third_party/spirv-tools \
    SPVHEADERS_LOCAL_PATH=$ROOT_DIR/third_party/spirv-headers \
    -j8 $@
}

# Builds all the ABIs (see APP_ABI in jni/Application.mk)
do_ndk_build

# Check that libshaderc_combined builds
# Explicitly set each ABI, otherwise it will only pick x86.
# It seems to be the behaviour when specifying an explicit target.
for abi in x86 x86_64 armeabi-v7a arm64-v8a; do
  do_ndk_build APP_ABI=$abi libshaderc_combined
done

echo $(date): ndk-build completed.
