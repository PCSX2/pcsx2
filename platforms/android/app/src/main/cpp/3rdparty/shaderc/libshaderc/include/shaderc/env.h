// Copyright 2018 The Shaderc Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SHADERC_ENV_H_
#define SHADERC_ENV_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  shaderc_target_env_vulkan,  // SPIR-V under Vulkan semantics
  shaderc_target_env_opengl,  // SPIR-V under OpenGL semantics
  // NOTE: SPIR-V code generation is not supported for shaders under OpenGL
  // compatibility profile.
  shaderc_target_env_opengl_compat,  // SPIR-V under OpenGL semantics,
                                     // including compatibility profile
                                     // functions
  shaderc_target_env_webgpu,         // Deprecated, SPIR-V under WebGPU
                                     // semantics
  shaderc_target_env_default = shaderc_target_env_vulkan
} shaderc_target_env;

typedef enum {
  // For Vulkan, use Vulkan's mapping of version numbers to integers.
  // See vulkan.h
  shaderc_env_version_vulkan_1_0 = ((1u << 22)),
  shaderc_env_version_vulkan_1_1 = ((1u << 22) | (1 << 12)),
  shaderc_env_version_vulkan_1_2 = ((1u << 22) | (2 << 12)),
  shaderc_env_version_vulkan_1_3 = ((1u << 22) | (3 << 12)),
  shaderc_env_version_vulkan_1_4 = ((1u << 22) | (4 << 12)),
  // For OpenGL, use the number from #version in shaders.
  // TODO(dneto): Currently no difference between OpenGL 4.5 and 4.6.
  // See glslang/Standalone/Standalone.cpp
  // TODO(dneto): Glslang doesn't accept a OpenGL client version of 460.
  shaderc_env_version_opengl_4_5 = 450,
  shaderc_env_version_webgpu,  // Deprecated, WebGPU env never defined versions
} shaderc_env_version;

// The known versions of SPIR-V.
typedef enum {
  // Use the values used for word 1 of a SPIR-V binary:
  // - bits 24 to 31: zero
  // - bits 16 to 23: major version number
  // - bits 8 to 15: minor version number
  // - bits 0 to 7: zero
  shaderc_spirv_version_1_0 = 0x010000u,
  shaderc_spirv_version_1_1 = 0x010100u,
  shaderc_spirv_version_1_2 = 0x010200u,
  shaderc_spirv_version_1_3 = 0x010300u,
  shaderc_spirv_version_1_4 = 0x010400u,
  shaderc_spirv_version_1_5 = 0x010500u,
  shaderc_spirv_version_1_6 = 0x010600u
} shaderc_spirv_version;

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SHADERC_ENV_H_
