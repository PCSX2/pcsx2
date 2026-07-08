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

ROOT_SHADERC_PATH := $(call my-dir)

include $(ROOT_SHADERC_PATH)/third_party/Android.mk
include $(ROOT_SHADERC_PATH)/libshaderc_util/Android.mk
include $(ROOT_SHADERC_PATH)/libshaderc/Android.mk

ALL_LIBS:=libglslang.a \
	libshaderc.a \
	libshaderc_util.a \
	libSPIRV.a \
	libSPIRV-Tools.a \
	libSPIRV-Tools-opt.a

SHADERC_HEADERS=shaderc.hpp shaderc.h env.h status.h visibility.h
SHADERC_HEADERS_IN_OUT_DIR=$(foreach H,$(SHADERC_HEADERS),$(NDK_APP_LIBS_OUT)/../include/shaderc/$(H))

define gen_libshaderc_header
$(call generate-file-dir,$(NDK_APP_LIBS_OUT)/../include/shaderc/$(1))
$(NDK_APP_LIBS_OUT)/../include/shaderc/$(1) : \
		$(ROOT_SHADERC_PATH)/libshaderc/include/shaderc/$(1)
	$(call host-cp,$(ROOT_SHADERC_PATH)/libshaderc/include/shaderc/$(1) \
		,$(NDK_APP_LIBS_OUT)/../include/shaderc/$(1))

endef
# Generate headers
$(eval $(foreach H,$(SHADERC_HEADERS),$(call gen_libshaderc_header,$(H))))
libshaderc_headers: $(SHADERC_HEADERS_IN_OUT_DIR)
.PHONY: libshaderc_headers


# Rules for combining library files to form a single libshader_combined.a.
# It always goes into $(TARGET_OUT)
$(call generate-file-dir,$(TARGET_OUT)/combine.ar)
$(TARGET_OUT)/combine.ar: $(TARGET_OUT) $(addprefix $(TARGET_OUT)/, $(ALL_LIBS))
	$(file >$(TARGET_OUT)/combine.ar,create libshaderc_combined.a)
	$(foreach lib,$(ALL_LIBS),$(file >>$(TARGET_OUT)/combine.ar,addlib $(lib)))
	$(file >>$(TARGET_OUT)/combine.ar,save)
	$(file >>$(TARGET_OUT)/combine.ar,end)

$(TARGET_OUT)/libshaderc_combined.a: $(addprefix $(TARGET_OUT)/, $(ALL_LIBS)) $(TARGET_OUT)/combine.ar
	@echo "[$(TARGET_ARCH_ABI)] Combine: libshaderc_combined.a <= $(ALL_LIBS)"
	@cd $(TARGET_OUT) && $(TARGET_AR) -M < combine.ar && cd $(ROOT_SHADERC_PATH)
	@$(TARGET_STRIP) --strip-debug $(TARGET_OUT)/libshaderc_combined.a

$(call generate-file-dir,$(NDK_APP_LIBS_OUT)/$(APP_STL)/$(TARGET_ARCH_ABI)/libshaderc.a)
$(NDK_APP_LIBS_OUT)/$(APP_STL)/$(TARGET_ARCH_ABI)/libshaderc.a: \
		$(TARGET_OUT)/libshaderc_combined.a
	$(call host-cp,$(TARGET_OUT)/libshaderc_combined.a \
		,$(NDK_APP_LIBS_OUT)/$(APP_STL)/$(TARGET_ARCH_ABI)/libshaderc.a)

libshaderc_combined: libshaderc_headers \
	$(NDK_APP_LIBS_OUT)/$(APP_STL)/$(TARGET_ARCH_ABI)/libshaderc.a
