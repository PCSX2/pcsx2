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
LOCAL_CPP_EXTENSION := .cc .cpp .cxx
LOCAL_MODULE:=shaderc
LOCAL_EXPORT_C_INCLUDES:=$(LOCAL_PATH)/include
LOCAL_SRC_FILES:=src/shaderc.cc
# The Shaderc third_party/Android.mk deduces SPVHEADERS_LOCAL_PATH,
# or delegates that responsibility to SPIRV-Tools' Android.mk.
LOCAL_C_INCLUDES:=$(LOCAL_PATH)/include $(SPVHEADERS_LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES:=shaderc_util SPIRV-Tools-opt
LOCAL_CXXFLAGS:=-std=c++17 -fno-exceptions -fno-rtti -DENABLE_HLSL=1
LOCAL_EXPORT_CPPFLAGS:=-std=c++17
LOCAL_EXPORT_LDFLAGS:=-latomic
include $(BUILD_STATIC_LIBRARY)
