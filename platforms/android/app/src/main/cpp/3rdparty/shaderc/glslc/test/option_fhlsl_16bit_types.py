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

HLSL_SHADER_WITH_HALF_TYPE = """
float4 main() : SV_Target0 {
  half h0 = (half)0.0;
  half h1 = (half)1.0;
  half h2 = (half)2.0;
  half h3 = (half)3.0;
  half4 v = (half4)(h0,h1,h2,h3) * (half)2.0;
  return (float4)(v);
}
"""


@inside_glslc_testsuite('OptionFHlsl16BitTypes')
class TestHlsl16BitTypes_EnablesCapability(expect.ValidAssemblyFileWithSubstr):
    """Tests that -fhlsl_16bit_types enables the 16bit floating point capability."""

    shader = FileShader(HLSL_SHADER_WITH_HALF_TYPE, '.frag')
    glslc_args = ['-S', '-x', 'hlsl', '-fhlsl-16bit-types', '-fauto-bind-uniforms', shader]
    expected_assembly_substr = 'OpCapability Float16';


@inside_glslc_testsuite('OptionFHlsl16BitTypes')
class TestHlsl16BitTypes_CreatesType(expect.ValidAssemblyFileWithSubstr):
    """Tests that -fhlsl_16bit_types creates the 16bit floating point capability."""

    shader = FileShader(HLSL_SHADER_WITH_HALF_TYPE, '.frag')
    glslc_args = ['-S', '-x', 'hlsl', '-fhlsl-16bit-types', '-fauto-bind-uniforms', shader]
    expected_assembly_substr = '= OpTypeFloat 16';
