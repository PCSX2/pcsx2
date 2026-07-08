# Copyright 2020 The Shaderc Authors. All rights reserved.
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

project(libshaderc_util)

add_library(shaderc_util STATIC
  include/libshaderc_util/counting_includer.h
  include/libshaderc_util/file_finder.h
  include/libshaderc_util/format.h
  include/libshaderc_util/io_shaderc.h
  include/libshaderc_util/mutex.h
  include/libshaderc_util/message.h
  include/libshaderc_util/resources.h
  include/libshaderc_util/spirv_tools_wrapper.h
  include/libshaderc_util/string_piece.h
  include/libshaderc_util/universal_unistd.h
  include/libshaderc_util/version_profile.h
  src/args.cc
  src/compiler.cc
  src/file_finder.cc
  src/io_shaderc.cc
  src/message.cc
  src/resources.cc
  src/shader_stage.cc
  src/spirv_tools_wrapper.cc
  src/version_profile.cc
)

shaderc_default_compile_options(shaderc_util)
target_include_directories(shaderc_util
  PUBLIC include PRIVATE ${glslang_SOURCE_DIR})
# We use parts of Glslang's HLSL compilation interface, which
# now requires this preprocessor definition.
add_definitions(-DENABLE_HLSL)

find_package(Threads)
target_link_libraries(shaderc_util PRIVATE
  glslang SPIRV
  SPIRV-Tools-opt ${CMAKE_THREAD_LIBS_INIT})

shaderc_add_tests(
  TEST_PREFIX shaderc_util
  LINK_LIBS shaderc_util
  TEST_NAMES
    counting_includer
    string_piece
    format
    file_finder
    io_shaderc
    message
    mutex
    version_profile)

if(${SHADERC_ENABLE_TESTS})
  target_include_directories(shaderc_util_counting_includer_test
    PRIVATE ${glslang_SOURCE_DIR})
  target_include_directories(shaderc_util_version_profile_test
    PRIVATE ${glslang_SOURCE_DIR})
endif()

shaderc_add_tests(
  TEST_PREFIX shaderc_util
  LINK_LIBS shaderc_util
  INCLUDE_DIRS
    ${glslang_SOURCE_DIR}
    ${spirv-tools_SOURCE_DIR}/include
  TEST_NAMES
    compiler)

# This target copies content of testdata into the build directory.
add_custom_target(testdata COMMAND
  ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/testdata/copy-to-build.cmake
  COMMENT "Copy testdata into build directory")

if(${SHADERC_ENABLE_TESTS})
  add_dependencies(shaderc_util_file_finder_test testdata)
  add_dependencies(shaderc_util_io_shaderc_test testdata)
endif()
