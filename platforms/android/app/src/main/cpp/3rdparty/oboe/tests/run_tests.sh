#!/bin/bash

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
# - CMake on PATH. This is usually found in $ANDROID_HOME/cmake/<version>/bin.
# - ANDROID_NDK environment variable is set to your Android NDK location
# e.g. $HOME/Library/Android/sdk/ndk/<version>
# - Android device or emulator attached and accessible via adb
#
# Instructions:
# - Run this script 
# - Check the test results on your target device
#
# What does the script do?
# - Builds a test binary for the target architecture
# - Copies the test binary into the UnitTestRunner app
# - Builds, installs and runs the app on the target device
#
# The initial run may take some time as GTest is built, subsequent runs should be much faster. 
#
# If you want to perform a clean build just delete the 'build' folder and re-run this script. You will need to do 
# this if you change target architectures (e.g. when changing between real device and emulator)
#
# Why is running the tests so convoluted? 
# The tests require the RECORDING permission and on many devices (e.g Samsung) the adb user does not have this 
# permission (and `run-as` is broken). This means that the test binary must be executed by an app which has this 
# permission, hence the need for the UnitTestRunner app. 
# 
################################################

# Directories, paths and filenames
BUILD_DIR=build
CMAKE=cmake
TEST_BINARY_FILENAME=testOboe
TEST_RUNNER_DIR=UnitTestRunner
TEST_RUNNER_PACKAGE_NAME=com.google.oboe.tests.unittestrunner
TEST_RUNNER_JNILIBS_DIR=${TEST_RUNNER_DIR}/app/src/main/jniLibs
TEST_RUNNER_ASSETS_DIR=${TEST_RUNNER_DIR}/app/src/main/assets

# Check prerequisites
if [ -z "$ANDROID_NDK" ]; then
    echo "Please set ANDROID_NDK to the Android NDK folder"
    exit 1
fi

if [ ! $(type -P ${CMAKE}) ]; then
    echo "${CMAKE} was not found on your path. You can install it using Android Studio using Tools->Android->SDK Manager->SDK Tools."
    echo "Once done you will need to add ${HOME}/Library/Android/sdk/cmake/<current_version>/bin to your path."
    exit 1
fi 

# Get the device ABI
ABI=$(adb shell getprop ro.product.cpu.abi | tr -d '\n\r')

if [ -z "$ABI" ]; then
    echo "No device ABI was set. Please ensure a device or emulator is running. You may need to unplug extra devices."
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

mkdir -p ${BUILD_DIR} 

echo "Cleaning up previous build because swapping phones may result in stale binaries"
rm -r ${BUILD_DIR}

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

# Copy the binary into the jniLibs and assets folders of the unit test runner app
# The assets folder does not work after Android R for security reasons
# The jniLibs folder doesn't seem to work before Android O
# Thus, copy into both
mkdir ${TEST_RUNNER_JNILIBS_DIR}
mkdir ${TEST_RUNNER_JNILIBS_DIR}/${ABI}
DESTINATION_DIR=${TEST_RUNNER_JNILIBS_DIR}/${ABI}/${TEST_BINARY_FILENAME}
echo "Copying binary to ${DESTINATION_DIR}"
cp ${BUILD_DIR}/${TEST_BINARY_FILENAME} ${DESTINATION_DIR}.so
mkdir ${TEST_RUNNER_ASSETS_DIR}
mkdir ${TEST_RUNNER_ASSETS_DIR}/${ABI}
DESTINATION_DIR=${TEST_RUNNER_ASSETS_DIR}/${ABI}/${TEST_BINARY_FILENAME}
echo "Copying binary to ${DESTINATION_DIR}"
cp ${BUILD_DIR}/${TEST_BINARY_FILENAME} ${DESTINATION_DIR}.so

# Build and install the unit test runner app
pushd ${TEST_RUNNER_DIR}
    echo "Building test runner app" 
	./gradlew assembleDebug
	if [ $? -ne 0 ]; then
		echo "Building test app FAILED"
		exit 1
	fi
	
	echo "Installing to device"
	./gradlew installDebug
	if [ $? -ne 0 ]; then
		echo "Installing tests FAILED"
		exit 1
	fi
popd

echo "Clear logcat from before the test."
adb logcat -c
echo "Starting app - Check your device for test results"
adb shell am start ${TEST_RUNNER_PACKAGE_NAME}/.MainActivity 

sleep 1
echo "Logging test logs and Oboe logs. Run adb logcat for complete logs."
adb logcat ${TEST_RUNNER_PACKAGE_NAME}.MainActivity:V OboeAudio:V *:S
