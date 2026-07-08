# Copyright 2020 The Shaderc Authors. All rights reserved.
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

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:=shaderc_util
LOCAL_CXXFLAGS:=-std=c++17 -fno-exceptions -fno-rtti -DENABLE_HLSL=1
LOCAL_EXPORT_C_INCLUDES:=$(LOCAL_PATH)/include
LOCAL_SRC_FILES:=src/args.cc \
                src/compiler.cc \
		src/file_finder.cc \
		src/io_shaderc.cc \
		src/message.cc \
		src/resources.cc \
		src/shader_stage.cc \
		src/spirv_tools_wrapper.cc \
		src/version_profile.cc
LOCAL_STATIC_LIBRARIES:=SPIRV SPIRV-Tools-opt glslang
LOCAL_C_INCLUDES:=$(LOCAL_PATH)/include
include $(BUILD_STATIC_LIBRARY)
