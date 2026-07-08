# Copyright 2026 The Shaderc Authors. All rights reserved.
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

import expect
from glslc_test_framework import inside_glslc_testsuite
from placeholder import FileShader

# See GL_EXT_descriptor_heap
# https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_descriptor_heap.txt
GLSL_COMPUTE_SHADER_DESCRIPTOR_HEAP_BUFFER = """#version 450
#extension GL_EXT_descriptor_heap : require

layout(descriptor_heap) uniform U { uint source; } ubo[];
layout(descriptor_heap) buffer B { uint dest; } ssbo[];

void main()
{
  ssbo[42].dest = ubo[2026].source;
}"""

@inside_glslc_testsuite('GL_EXT_descriptor_heap')
class BufferSampleCompiles(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_COMPUTE_SHADER_DESCRIPTOR_HEAP_BUFFER, '.comp')
    glslc_args = ['-S', shader, '--target-env=vulkan1.2']
    expected_assembly_substrings = [
            "OpCapability UntypedPointersKHR",
            "OpCapability DescriptorHeapEXT",
            "= OpUntypedAccessChainKHR",
            "= OpBufferPointerEXT",
    ]
