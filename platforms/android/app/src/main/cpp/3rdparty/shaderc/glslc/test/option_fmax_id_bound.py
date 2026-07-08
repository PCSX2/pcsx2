# Copyright 2025 The Shaderc Authors. All rights reserved.
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

COMPUTE_SHADER = """#version 450
#extension GL_EXT_control_flow_attributes : require

layout(local_size_x=1) in;

layout(binding=0) buffer Out { uint x; } g_out;

void main() {
    uint x = 0;
    [[unroll]]
    for (int i = 0; i < 100; ++i) {
        x = x + uint(i);
    }
    g_out.x = x;
}
"""

@inside_glslc_testsuite('OptionFMaxIdBound')
class TestFMaxIdBoundLow(expect.ErrorMessageSubstr):
    """Tests that compilation fails with a low -fmax-id-bound."""

    shader = FileShader(COMPUTE_SHADER, ".comp")
    glslc_args = ['-c', shader, '-fmax-id-bound=300', '-O']
    expected_error_substr = [" ID overflow. Try running compact-ids"]


@inside_glslc_testsuite('OptionFMaxIdBound')
class TestFMaxIdBoundHigh(expect.ValidObjectFile):
    """Tests that compilation passes with a high -fmax-id-bound."""

    shader = FileShader(COMPUTE_SHADER, ".comp")
    glslc_args = ['-c', shader, '-fmax-id-bound=200000', '-O']
