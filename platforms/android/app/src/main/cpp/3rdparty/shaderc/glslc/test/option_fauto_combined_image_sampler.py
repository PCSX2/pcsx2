# Copyright 2018 The Shaderc Authors. All rights reserved.
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

# A GLSL shader with a separate sampler and texture2D object
GLSL_SHADER_SEPARATE_IMAGE_SAMPLER = """#version 460
  layout (location=0) in vec2 in_UV;
  layout (location=0) out vec4 out_Color;
  layout (set=0,binding=0) uniform sampler u_Sampler;
  layout (set=0,binding=0) uniform texture2D u_Tex;
  void main() {
    out_Color = texture(sampler2D(u_Tex, u_Sampler), in_UV);
  }"""


# An HLSL fragment shader with the usual Texture2D and SamplerState pair
HLSL_SHADER_SEPARATE_IMAGE_SAMPLER = """
  Texture2D u_Tex;
  SamplerState u_Sampler;
  float4 Frag(float2 uv) : COLOR0 {
    return u_Tex.Sample(u_Sampler, uv);
  }"""


@inside_glslc_testsuite('OptionFAutoCombinedImageSampler')
class FAutoCombinedImageSamplerCheckGLSL(expect.ValidAssemblyFileWithSubstr):
    """Tests that the compiler combines GLSL sampler and texture2D objects."""
    shader = FileShader(GLSL_SHADER_SEPARATE_IMAGE_SAMPLER, '.frag')
    glslc_args = ['-S', '-fauto-combined-image-sampler', shader]
    expected_assembly_substr = """%10 = OpTypeImage %float 2D 0 0 0 1 Unknown
         %11 = OpTypeSampledImage %10
%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11
      %u_Tex = OpVariable %_ptr_UniformConstant_11 UniformConstant"""

@inside_glslc_testsuite('OptionFAutoCombinedImageSampler')
class FAutoCombinedImageSamplerCheckHLSL(expect.ValidAssemblyFileWithSubstr):
    """Tests that the HLSL compiler combines HLSL Texture2D and SamplerState objects into SPIRV SampledImage."""

    shader = FileShader(HLSL_SHADER_SEPARATE_IMAGE_SAMPLER, '.hlsl')
    glslc_args = ['-S', '-fshader-stage=frag', '-fentry-point=Frag', '-fauto-combined-image-sampler', shader]
    expected_assembly_substr = """%14 = OpTypeImage %float 2D 0 0 0 1 Unknown
         %15 = OpTypeSampledImage %14
%_ptr_UniformConstant_15 = OpTypePointer UniformConstant %15
      %u_Tex = OpVariable %_ptr_UniformConstant_15 UniformConstant"""

