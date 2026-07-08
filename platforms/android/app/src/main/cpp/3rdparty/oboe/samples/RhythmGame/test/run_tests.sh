# Copyright 2018 The Android Open Source Project
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

################################################
# Script to build and run the Oboe tests on an attached Android device or emulator
#
# Prerequisites:
# - CMake on PATH
# - ANDROID_NDK environment variable is set to your Android NDK location
# e.g. /Library/Android/sdk/ndk-bundle
# - Android device or emulator attached and accessible via adb
#
# Instructions:
# Run this script from within the oboe/tests directory. A directory 'build' will be
# created containing the test binary. This binary will then be copied to the device/emulator
# and executed.
#
# The initial run may take some time as GTest is built, subsequent runs should be much, much
# faster.
#
# If you want to perform a clean build just delete the 'build' folder and re-run this script
#
################################################

#!/bin/bash

# Directories, paths and filenames
BUILD_DIR=build
CMAKE=cmake
TEST_BINARY_FILENAME=testRhythmGame
REMOTE_DIR=/data/local/tmp/RhythmGame

# Check prerequisites
if [ -z "$ANDROID_NDK" ]; then
    echo "Please set ANDROID_NDK to the Android NDK folder"
    exit 1
fi

if [ ! $(type -P ${CMAKE}) ]; then
	echo "${CMAKE} was not found on your path. You can install it using Android Studio using Tools->Android->SDK Manager->SDK Tools."
	exit 1
fi

# Get the device ABI
ABI=$(adb shell getprop ro.product.cpu.abi | tr -d '\n\r')

if [ -z "$ABI" ]; then
    echo "No device ABI was set. Please ensure a device or emulator is running"
    exit 1
fi

echo "Device/emulator architecture is $ABI"

if [ ${ABI} == "arm64-v8a" ] || [ ${ABI} == "x86_64" ]; then
	PLATFORM=android-21
elif [ ${ABI} == "armeabi-v7a" ] || [ ${ABI} == "x86" ]; then
	PLATFORM=android-16
else
	echo "Unrecognised ABI: ${ABI}. Supported ABIs are: arm64-v8a, armeabi-v7a, x86_64, x86. If you feel ${ABI} should be supported please file an issue on github.com/google/oboe"
	exit 1
fi

# Configure the build
echo "Building tests for ${ABI} using ${PLATFORM}"

CMAKE_ARGS="-S. \
	-B${BUILD_DIR} \
	-DANDROID_ABI=${ABI} \
	-DANDROID_PLATFORM=${PLATFORM} \
  	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_CXX_FLAGS=-std=c++17 \
	-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
	-DCMAKE_VERBOSE_MAKEFILE=1"

mkdir -p ${BUILD_DIR}

cmake ${CMAKE_ARGS}

# Perform the build
pushd ${BUILD_DIR}
    make -j5

	if [ $? -eq 0 ]; then
		echo "Tests built successfully"
	else
		echo "Building tests FAILED"
		exit 1
	fi

popd


# Push the test binary to the device and run it
echo "Pushing test binary to device/emulator"
adb shell mkdir -p ${REMOTE_DIR}
adb push ${BUILD_DIR}/${TEST_BINARY_FILENAME} ${REMOTE_DIR}

echo "Running test binary"
adb shell ${REMOTE_DIR}/${TEST_BINARY_FILENAME}
