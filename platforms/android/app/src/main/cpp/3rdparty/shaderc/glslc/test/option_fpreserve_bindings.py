# Copyright 2023 The Shaderc Authors. All rights reserved.
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

# A GLSL shader with unused bindings.
GLSL_SHADER_WITH_UNUSED_BINDINGS = """#version 450
  layout(binding=0) buffer InputA { vec4 x[]; } inputA;
  layout(binding=1) buffer InputB { vec4 x[]; } inputB;
  layout(binding=2) buffer Output { vec4 x[]; } outputO;

  void main() {}
  """


@inside_glslc_testsuite('OptionFPreserveBindings')
class TestFPreserveBindingsInputA(expect.ValidAssemblyFileWithSubstr):
    """Tests that the compiler preserves bindings when optimizations are
    enabled."""

    shader = FileShader(GLSL_SHADER_WITH_UNUSED_BINDINGS, '.comp')
    glslc_args = ['-S', '-O', shader, '-fpreserve-bindings']
    # Check the first buffer.
    expected_assembly_substr = "Binding 0"


@inside_glslc_testsuite('OptionFPreserveBindings')
class TestFPreserveBindingsInputB(expect.ValidAssemblyFileWithSubstr):
    """Tests that the compiler preserves bindings when optimizations are
    enabled."""

    shader = FileShader(GLSL_SHADER_WITH_UNUSED_BINDINGS, '.comp')
    glslc_args = ['-S', '-O', shader, '-fpreserve-bindings']
    # Check the first buffer.
    expected_assembly_substr = "Binding 1"


@inside_glslc_testsuite('OptionFPreserveBindings')
class TestFPreserveBindingsOutputO(expect.ValidAssemblyFileWithSubstr):
    """Tests that the compiler preserves bindings when optimizations are
    enabled."""

    shader = FileShader(GLSL_SHADER_WITH_UNUSED_BINDINGS, '.comp')
    glslc_args = ['-S', '-O', shader, '-fpreserve-bindings']
    # Check the first buffer.
    expected_assembly_substr = "Binding 2"


@inside_glslc_testsuite('OptionFPreserveBindings')
class TestFNoPreserveBindings(expect.ValidAssemblyFileWithoutSubstr):
    """Tests that the compiler removes bindings when -fpreserve-bindings is not
    set."""

    shader = FileShader(GLSL_SHADER_WITH_UNUSED_BINDINGS, '.comp')
    glslc_args = ['-S', '-O', shader]
    # Check that all binding decorations are gone.
    unexpected_assembly_substr = "OpDecorate"
