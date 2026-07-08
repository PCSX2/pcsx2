# Copyright 2019 The Shaderc Authors. All rights reserved.
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

# A GLSL shader using the clamp, max, and min builtin functions.
GLSL_FRAG_SHADER_WITH_CLAMP = """#version 450
layout(location=0) in vec4 i;
layout(location=0) out vec4 o;
void main() {
  o = clamp(i, vec4(0.5), vec4(1.0))
  +   max(i, vec4(0.5))
  +   min(i, vec4(0.5));
}
"""


@inside_glslc_testsuite('OptionFNanClamp')
class TestClampMapsToFClampByDefault(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_FRAG_SHADER_WITH_CLAMP, '.frag')
    glslc_args = ['-S', shader]
    expected_assembly_substr = 'OpExtInst %v4float %1 FClamp'


@inside_glslc_testsuite('OptionFNanClamp')
class TestMaxMapsToFMaxByDefault(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_FRAG_SHADER_WITH_CLAMP, '.frag')
    glslc_args = ['-S', shader]
    expected_assembly_substr = 'OpExtInst %v4float %1 FMax'


@inside_glslc_testsuite('OptionFNanClamp')
class TestMinMapsToFMinByDefault(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_FRAG_SHADER_WITH_CLAMP, '.frag')
    glslc_args = ['-S', shader]
    expected_assembly_substr = 'OpExtInst %v4float %1 FMin'


@inside_glslc_testsuite('OptionFNanClamp')
class TestClampMapsToNClampWithFlag(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_FRAG_SHADER_WITH_CLAMP, '.frag')
    glslc_args = ['-S', '-fnan-clamp', shader]
    expected_assembly_substr = 'OpExtInst %v4float %1 NClamp'

@inside_glslc_testsuite('OptionFNanClamp')
class TestMaxMapsToNMaxWithFlag(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_FRAG_SHADER_WITH_CLAMP, '.frag')
    glslc_args = ['-S', '-fnan-clamp', shader]
    expected_assembly_substr = 'OpExtInst %v4float %1 NMax'


@inside_glslc_testsuite('OptionFNanClamp')
class TestMinMapsToNMinWithFlag(expect.ValidAssemblyFileWithSubstr):
    shader = FileShader(GLSL_FRAG_SHADER_WITH_CLAMP, '.frag')
    glslc_args = ['-S', '-fnan-clamp', shader]
    expected_assembly_substr = 'OpExtInst %v4float %1 NMin'
