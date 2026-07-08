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


def vulkan_vertex_shader():
    return """#version 310 es
void main() { int t = gl_VertexIndex; }"""


def vulkan_compute_subgroup_shader():
    """Returns a compute shader that requires Vulkan 1.1 and SPIR-V 1.3"""
    return """#version 450
              #extension GL_KHR_shader_subgroup_basic : enable
              void main() { subgroupBarrier(); }"""


@inside_glslc_testsuite('OptionTargetSpv')
class TestDefaultTargetSpvWithVulkanShader(expect.ValidObjectFile):
    """Tests that compiling a Vulkan-specific shader with a default
    target environment succeeds"""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['-c', shader]


@inside_glslc_testsuite('OptionTargetSpv')
class TestDefaultTargetSpvWithShaderRequiringSpv1p3Fails(expect.ErrorMessageSubstr):
    """Tests that compiling a shader requiring SPIR-V 1.3 with default SPIR-V
    target should fail.
    """
    shader = FileShader(vulkan_compute_subgroup_shader(), '.comp')
    glslc_args = ['-c', shader]
    expected_error_substr = ["error: 'subgroup op' : requires SPIR-V 1.3\n"]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpv1p2WithShaderRequiringSpv1p3Fails(expect.ErrorMessageSubstr):
    """Tests that compiling a shader requiring SPIR-V 1.3 but targeting 1.2
    should fail.
    """
    shader = FileShader(vulkan_compute_subgroup_shader(), '.comp')
    glslc_args = ['--target-spv=spv1.2', '-c', shader]
    expected_error_substr = ["error: 'subgroup op' : requires SPIR-V 1.3\n"]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpv1p3(expect.ValidObjectFile1_3):
    """Tests that compiling to spv1.3 succeeds and generates SPIR-V 1.3 binary."""
    shader = FileShader(vulkan_compute_subgroup_shader(), '.comp')
    glslc_args = ['--target-spv=spv1.3', '-c', shader]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpv1p4(expect.ValidObjectFile1_4):
    """Tests that compiling to spv1.4 succeeds and generates SPIR-V 1.4 binary."""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv=spv1.4', '-c', shader]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpv1p5(expect.ValidObjectFile1_5):
    """Tests that compiling to spv1.5 succeeds and generates SPIR-V 1.5 binary."""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv=spv1.5', '-c', shader]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpv1p5(expect.ValidObjectFile1_6):
    """Tests that compiling to spv1.6 succeeds and generates SPIR-V 1.6 binary."""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv=spv1.6', '-c', shader]


### Option parsing error cases

@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpvNoArg(expect.ErrorMessage):
    """Tests the error message of assigning empty string to --target-spv"""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv=', shader]
    expected_error = ["glslc: error: invalid value ",
                      "'' in '--target-spv='\n"]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpvNoEqNoArg(expect.ErrorMessage):
    """Tests the error message of using --target-spv without equal sign and
    arguments"""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv', shader]
    expected_error = ["glslc: error: unsupported option: ",
                      "'--target-spv'\n"]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpvNoEqWithArg(expect.ErrorMessage):
    """Tests the error message of using --target-spv without equal sign but
    arguments"""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv', 'spv1.3', shader]
    expected_error = ["glslc: error: unsupported option: ",
                      "'--target-spv'\n"]


@inside_glslc_testsuite('OptionTargetSpv')
class TestTargetSpvEqWrongArg(expect.ErrorMessage):
    """Tests the error message of using --target-spv with wrong argument"""
    shader = FileShader(vulkan_vertex_shader(), '.vert')
    glslc_args = ['--target-spv=wrong_arg', shader]
    expected_error = ["glslc: error: invalid value ",
                      "'wrong_arg' in '--target-spv=wrong_arg'\n"]
