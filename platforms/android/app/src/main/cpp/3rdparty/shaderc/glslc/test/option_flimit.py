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


def shader_source_with_tex_offset(offset):
    """Returns a vertex shader using a texture access with the given offset."""

    return """#version 450
              layout (binding=0) uniform sampler1D tex;
              void main() { vec4 x = textureOffset(tex, 1.0, """ + str(offset) + "); }"


def shader_with_tex_offset(offset):
    """Returns a vertex FileShader using a texture access with the given offset."""

    return FileShader(shader_source_with_tex_offset(offset), ".vert")

@inside_glslc_testsuite('OptionFLimit')
class TestFLimitNoEqual(expect.ErrorMessage):
    """Tests -flimit without equal."""

    glslc_args = ['-flimit']
    expected_error = ["glslc: error: unknown argument: '-flimit'\n"]


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitJustEqual(expect.ValidObjectFile):
    """Tests -flimit= with no argument."""

    shader = shader_with_tex_offset(0);
    glslc_args = ['-c', shader, '-flimit=']


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitJustEqualMaxOffset(expect.ValidObjectFile):
    """Tests -flimit= with no argument.  The shader uses max offset."""

    shader = shader_with_tex_offset(7);
    glslc_args = ['-c', shader, '-flimit=']


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitJustEqualMinOffset(expect.ValidObjectFile):
    """Tests -flimit= with no argument.  The shader uses min offset."""

    shader = shader_with_tex_offset(-8);
    glslc_args = ['-c', shader, '-flimit=']


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitJustEqualBelowMinOffset(expect.ErrorMessageSubstr):
    """Tests -flimit= with no argument.  The shader uses below min default offset."""

    shader = shader_with_tex_offset(-9);
    glslc_args = ['-c', shader, '-flimit=']
    expected_error_substr = ["'texel offset' : value is out of range"]


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitLowerThanDefaultMinOffset(expect.ValidObjectFile):
    """Tests -flimit= with lower than default argument.  The shader uses below min offset."""

    shader = shader_with_tex_offset(-9);
    glslc_args = ['-c', shader, '-flimit= MinProgramTexelOffset -9 ']


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitIgnoredLangFeatureSettingSample(expect.ValidObjectFile):
    """Tests -flimit= an ignored option."""

    shader = FileShader("#version 150\nvoid main() { while(true); }", '.vert')
    glslc_args = ['-c', shader, '-flimit=whileLoops 0']


@inside_glslc_testsuite('OptionFLimit')
class TestFLimitLowerThanDefaultMinOffset(expect.ValidObjectFile):
    """Tests -flimit= with lower than default argument.  The shader uses that offset."""

    shader = shader_with_tex_offset(-9);
    glslc_args = ['-c', shader, '-flimit= MinProgramTexelOffset -9 ']


@inside_glslc_testsuite('OptionFLimitFile')
class TestFLimitFileNoArg(expect.ErrorMessage):
    """Tests -flimit-file without an argument"""

    shader = shader_with_tex_offset(-9);
    glslc_args = ['-c', shader, '-flimit-file']
    expected_error = "glslc: error: argument to '-flimit-file' is missing\n"


@inside_glslc_testsuite('OptionFLimitFile')
class TestFLimitFileMissingFile(expect.ErrorMessageSubstr):
    """Tests -flimit-file without an argument"""

    shader = shader_with_tex_offset(-9);
    glslc_args = ['-c', shader, '-flimit-file', 'i do not exist']
    expected_error_substr = "glslc: error: cannot open input file: 'i do not exist'";


@inside_glslc_testsuite('OptionFLimitFile')
class TestFLimitFileSetsLowerMinTexelOffset(expect.ValidObjectFile):
    """Tests -flimit-file with lower than default argument.  The shader uses that offset."""

    limits_file = File('limits.txt', 'MinProgramTexelOffset -9')
    shader = File('shader.vert', shader_source_with_tex_offset(-9));
    environment = Directory('.', [limits_file, shader])
    glslc_args = ['-c', shader.name, '-flimit-file', limits_file.name]


@inside_glslc_testsuite('OptionFLimitFile')
class TestFLimitFileInvalidContents(expect.ErrorMessage):
    """Tests -flimit-file bad file contents."""

    limits_file = File('limits.txt', 'thisIsBad')
    shader = File('shader.vert', shader_source_with_tex_offset(-9));
    environment = Directory('.', [limits_file, shader])
    glslc_args = ['-c', shader.name, '-flimit-file', limits_file.name]
    expected_error = 'glslc: error: -flimit-file error: invalid resource limit: thisIsBad\n'

## Mesh shading

def mesh_shader_with_params(kwargs):
    """Returns a mesh shader as a FileShader, with given parameters"""
    import sys

    source = """#version 450
#extension {} : enable
      layout(local_size_x={}) in;
      layout(local_size_y={}) in;
      layout(local_size_z={}) in;
      layout(triangles) out;
      layout(max_vertices={}) out;
      layout(max_primitives={}) out;
      layout(triangles) out;
      void main() {{ }}
      """.format(kwargs['extension'],kwargs['x'],kwargs['y'],kwargs['z'],kwargs['max_vert'],kwargs['max_prim'])
    return FileShader(source,".mesh")

def task_shader_with_params(kwargs):
    """Returns a task shader as a FileShader, with given parameters"""
    import sys

    source = """#version 450
#extension {} : enable
      layout(local_size_x={}) in;
      layout(local_size_y={}) in;
      layout(local_size_z={}) in;
      void main() {{ }}
      """.format(kwargs['extension'],kwargs['x'],kwargs['y'],kwargs['z'])
    return FileShader(source,".task")


def meshDefaults(nv_or_ext,show=False):
  result = dict({
    # See Glslang's glslang/ResourceLimits/ResourceLimits.cpp
    'nv': { 'extension': 'GL_NV_mesh_shader', 'x': 32, 'y': 1, 'z': 1, 'max_vert': 256, 'max_prim': 512 },
    'ext': { 'extension': 'GL_EXT_mesh_shader', 'x': 128, 'y': 128, 'z': 128, 'max_vert': 256, 'max_prim': 256 }
  })[nv_or_ext]
  if show:
      import sys
      print(result,file=sys.stderr)
  return result

## GL_NV_mesh_shader

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_X_ok(expect.ValidObjectFile):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeX_NV 32']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_X_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    expected_error_substr = "'local_size' : too large, see gl_MaxMeshWorkGroupSizeNV"
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeX_NV 31']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_Y_ok(expect.ValidObjectFile):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeY_NV 1']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_Y_bad(expect.ErrorMessageSubstr):
    d = meshDefaults('nv')
    d['y'] = 3
    shader = mesh_shader_with_params(d)
    expected_error_substr = "'local_size' : too large, see gl_MaxMeshWorkGroupSizeNV"
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeY_NV 2']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_Z_ok(expect.ValidObjectFile):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeZ_NV 1']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_Z_bad(expect.ErrorMessageSubstr):
    d = meshDefaults('nv')
    d['z'] = 3
    shader = mesh_shader_with_params(d)
    expected_error_substr = "'local_size' : too large, see gl_MaxMeshWorkGroupSizeNV"
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeZ_NV 2']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_MaxVert_ok(expect.ValidObjectFile):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputVerticesNV 256']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_MaxVert_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    expected_error_substr = "'max_vertices' : too large, must be less than gl_MaxMeshOutputVerticesNV"
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputVerticesNV 255']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_MaxPrim_ok(expect.ValidObjectFile):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputPrimitivesNV 512']

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_NV_MaxPrim_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('nv'))
    expected_error_substr = "'max_primitives' : too large, must be less than gl_MaxMeshOutputPrimitivesNV"
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputPrimitivesNV 511']

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_NV_X_ok(expect.ValidObjectFile):
    shader = task_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeX_NV 32']

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_NV_X_bad(expect.ErrorMessageSubstr):
    shader = task_shader_with_params(meshDefaults('nv'))
    expected_error_substr = "'local_size' : too large, see gl_MaxTaskWorkGroupSizeNV"
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeX_NV 31']

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_NV_Y_ok(expect.ValidObjectFile):
    shader = task_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeY_NV 1']

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_NV_Y_bad(expect.ErrorMessageSubstr):
    d = meshDefaults('nv')
    d['y'] = 3
    shader = task_shader_with_params(d)
    expected_error_substr = "'local_size' : too large, see gl_MaxTaskWorkGroupSizeNV"
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeY_NV 2']

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_NV_Z_ok(expect.ValidObjectFile):
    shader = task_shader_with_params(meshDefaults('nv'))
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeZ_NV 1']

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_NV_Z_bad(expect.ErrorMessageSubstr):
    d = meshDefaults('nv')
    d['z'] = 3
    shader = task_shader_with_params(d)
    expected_error_substr = "'local_size' : too large, see gl_MaxTaskWorkGroupSizeNV"
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeZ_NV 2']

# TODO: Test MaxMeshViewCountNV


## GL_EXT_mesh_shader
## It requires SPIR-V 1.4

s14 = '--target-spv=spv1.4'

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_X_ok(expect.ValidObjectFile1_4):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeX_EXT 128', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_X_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'local_size' : too large, see gl_MaxMeshWorkGroupSizeEXT"
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeX_EXT 127', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_Y_ok(expect.ValidObjectFile1_4):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeY_EXT 128', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_Y_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'local_size' : too large, see gl_MaxMeshWorkGroupSizeEXT"
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeY_EXT 127', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_Z_ok(expect.ValidObjectFile1_4):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeZ_EXT 128', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_Z_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'local_size' : too large, see gl_MaxMeshWorkGroupSizeEXT"
    glslc_args = ['-c', shader, '-flimit= MaxMeshWorkGroupSizeZ_EXT 127', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_MaxVert_ok(expect.ValidObjectFile1_4):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputVerticesEXT 256', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_MaxVert_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'max_vertices' : too large, must be less than gl_MaxMeshOutputVerticesEXT"
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputVerticesEXT 255', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_MaxPrim_ok(expect.ValidObjectFile1_4):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputPrimitivesEXT 256', s14]

@inside_glslc_testsuite('OptionFLimit_Mesh')
class TestFLimitMeshShader_EXT_MaxPrim_bad(expect.ErrorMessageSubstr):
    shader = mesh_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'max_primitives' : too large, must be less than gl_MaxMeshOutputPrimitivesEXT"
    glslc_args = ['-c', shader, '-flimit= MaxMeshOutputPrimitivesEXT 255', s14]

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_EXT_X_ok(expect.ValidObjectFile1_4):
    shader = task_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeX_EXT 128', s14]

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_EXT_X_bad(expect.ErrorMessageSubstr):
    shader = task_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'local_size' : too large, see gl_MaxTaskWorkGroupSizeEXT"
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeX_EXT 127', s14]

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_EXT_Y_ok(expect.ValidObjectFile1_4):
    shader = task_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeY_EXT 128', s14]

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_EXT_Y_bad(expect.ErrorMessageSubstr):
    import sys
    d = meshDefaults('ext',True)
    print("TaskShader_EXT_Y_bad {}".format(str(d)),file=sys.stderr)
    shader = task_shader_with_params(meshDefaults('ext',True))
    expected_error_substr = "'local_size' : too large, see gl_MaxTaskWorkGroupSizeEXT"
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeY_EXT 127', s14]

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_EXT_Z_ok(expect.ValidObjectFile1_4):
    shader = task_shader_with_params(meshDefaults('ext'))
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeZ_EXT 128', s14]

@inside_glslc_testsuite('OptionFLimit_Task')
class TestFLimitTaskShader_EXT_Z_bad(expect.ErrorMessageSubstr):
    shader = task_shader_with_params(meshDefaults('ext'))
    expected_error_substr = "'local_size' : too large, see gl_MaxTaskWorkGroupSizeEXT"
    glslc_args = ['-c', shader, '-flimit= MaxTaskWorkGroupSizeZ_EXT 127', s14]

# TODO: Test MaxMeshViewCountEXT
