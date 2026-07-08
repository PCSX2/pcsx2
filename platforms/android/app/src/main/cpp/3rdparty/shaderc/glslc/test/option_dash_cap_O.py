# Copyright 2016 The Shaderc Authors. All rights reserved.
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
from environment import File, Directory
from glslc_test_framework import inside_glslc_testsuite
from placeholder import FileShader

MINIMAL_SHADER = '#version 310 es\nvoid main() {}'
EMPTY_SHADER_IN_CWD = Directory('.', [File('shader.vert', MINIMAL_SHADER)])

ASSEMBLY_WITH_DEBUG_SOURCE = [
    """; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 11
; Bound: 7
; Schema: 0
               OpCapability Shader
          %2 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
          %1 = OpString "shader.vert"
               OpSource ESSL 310 %1 "// OpModuleProcessed entry-point main
// OpModuleProcessed client vulkan100
// OpModuleProcessed target-env vulkan1.0
// OpModuleProcessed entry-point main
#line 1
#version 310 es
void main() {}"
               OpSourceExtension "GL_GOOGLE_cpp_style_line_directive"
               OpSourceExtension "GL_GOOGLE_include_directive"
               OpName %main "main"
       %void = OpTypeVoid
          %4 = OpTypeFunction %void
               OpLine %1 2 11
       %main = OpFunction %void None %4
          %6 = OpLabel
               OpLine %1 2 0
               OpReturn
               OpFunctionEnd
"""
]

ASSEMBLY_O0 = [
    """; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 11
; Bound: 6
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main"
               OpSource ESSL 310
               OpSourceExtension "GL_GOOGLE_cpp_style_line_directive"
               OpSourceExtension "GL_GOOGLE_include_directive"
               OpName %main "main"
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
"""]

ASSEMBLY_O = [
    """; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 11
; Bound: 6
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %4 "main"
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
          %4 = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
"""]

ASSEMBLY_Os = [
    """; SPIR-V
; Version: 1.0
; Generator: Google Shaderc over Glslang; 11
; Bound: 6
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %4 "main"
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
          %4 = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
"""]


@inside_glslc_testsuite('OptionDashCapO')
class TestDashCapO0(expect.ValidFileContents):
    """Tests that -O0 works."""

    environment = EMPTY_SHADER_IN_CWD
    glslc_args = ['-S', '-O0', 'shader.vert']
    target_filename = 'shader.vert.spvasm'
    expected_file_contents = ASSEMBLY_O0


@inside_glslc_testsuite('OptionDashCapO')
class TestDashCapOPerformance(expect.ValidFileContents):
    """Tests -O works."""

    environment = EMPTY_SHADER_IN_CWD
    glslc_args = ['-S', '-O', 'shader.vert']
    target_filename = 'shader.vert.spvasm'
    expected_file_contents = ASSEMBLY_O


@inside_glslc_testsuite('OptionDashCapO')
class TestDashCapOs(expect.ValidFileContents):
    """Tests that -Os works."""

    environment = EMPTY_SHADER_IN_CWD
    glslc_args = ['-S', '-Os', 'shader.vert']
    target_filename = 'shader.vert.spvasm'
    expected_file_contents = ASSEMBLY_Os


@inside_glslc_testsuite('OptionDashCapO')
class TestDashCapOOverriding(expect.ValidFileContents):
    """Tests that if there are multiple -O's, only the last one takes effect."""

    environment = EMPTY_SHADER_IN_CWD
    glslc_args = ['-S', '-Os', '-O0', '-Os', '-O0', 'shader.vert']
    target_filename = 'shader.vert.spvasm'
    expected_file_contents = ASSEMBLY_O0


@inside_glslc_testsuite('OptionDashCapO')
class TestDashCapOWithDashG(expect.ValidFileContents):
    """Tests that -g restrains -O from turning on strip debug info."""

    environment = EMPTY_SHADER_IN_CWD
    glslc_args = ['-S', '-Os', '-g', 'shader.vert']
    target_filename = 'shader.vert.spvasm'
    expected_file_contents = ASSEMBLY_WITH_DEBUG_SOURCE


@inside_glslc_testsuite('OptionDashCapO')
class TestDashGWithDashCapO(expect.ValidFileContents):
    """Tests that -g restrains -O from turning on strip debug info."""

    environment = EMPTY_SHADER_IN_CWD
    glslc_args = ['-S', '-g', '-Os', 'shader.vert']
    target_filename = 'shader.vert.spvasm'
    expected_file_contents = ASSEMBLY_WITH_DEBUG_SOURCE


@inside_glslc_testsuite('OptionDashCapO')
class TestWrongOptLevel(expect.NoGeneratedFiles, expect.ErrorMessage):
    """Tests erroring out with wrong optimization level."""

    shader = FileShader(MINIMAL_SHADER, '.vert')
    glslc_args = ['-c', '-O2', shader]
    expected_error = "glslc: error: invalid value '2' in '-O2'\n"
