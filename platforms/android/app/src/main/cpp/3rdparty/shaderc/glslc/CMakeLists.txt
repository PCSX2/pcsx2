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

find_package(Threads)

add_library(glslc STATIC
  src/file_compiler.cc
  src/file_compiler.h
  src/file.cc
  src/file.h
  src/file_includer.cc
  src/file_includer.h
  src/resource_parse.h
  src/resource_parse.cc
  src/shader_stage.cc
  src/shader_stage.h
  src/dependency_info.cc
  src/dependency_info.h
)

shaderc_default_compile_options(glslc)
target_include_directories(glslc PUBLIC ${glslang_SOURCE_DIR})

if (SHADERC_ENABLE_WGSL_OUTPUT)
  if (IS_DIRECTORY "${tint_SOURCE_DIR}/include")
    target_include_directories(glslc PRIVATE "${tint_SOURCE_DIR}/include")
    target_include_directories(glslc PRIVATE "${tint_SOURCE_DIR}")
  endif()
  # Turn on features in the tint/tint.h header
  add_definitions(-DTINT_BUILD_SPV_READER=1 -DTINT_BUILD_WGSL_WRITER=1)
  add_definitions(-DSHADERC_ENABLE_WGSL_OUTPUT=1)
endif(SHADERC_ENABLE_WGSL_OUTPUT)

target_link_libraries(glslc PRIVATE
  glslang SPIRV    # Glslang libraries
  $<$<BOOL:${SHADERC_ENABLE_WGSL_OUTPUT}>:libtint>      # Tint libraries, optional
  shaderc_util shaderc                                  # internal Shaderc libraries
  ${CMAKE_THREAD_LIBS_INIT})

if(SHADERC_ENABLE_EXECUTABLES)
  add_executable(glslc_exe src/main.cc)
  shaderc_default_compile_options(glslc_exe)
  target_include_directories(glslc_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/.. ${spirv-tools_SOURCE_DIR}/include)
  set_target_properties(glslc_exe PROPERTIES OUTPUT_NAME glslc)
  target_link_libraries(glslc_exe PRIVATE glslc shaderc_util shaderc)
  add_dependencies(glslc_exe build-version)
endif(SHADERC_ENABLE_EXECUTABLES)

shaderc_add_tests(
  TEST_PREFIX glslc
  LINK_LIBS glslc shaderc_util shaderc
  TEST_NAMES
    file
    resource_parse
    stage)

shaderc_add_asciidoc(glslc_doc_README README)

if(SHADERC_ENABLE_INSTALL)
  if(SHADERC_ENABLE_EXECUTABLES)
    install(TARGETS glslc_exe
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif(SHADERC_ENABLE_EXECUTABLES)
endif(SHADERC_ENABLE_INSTALL)

add_subdirectory(test)
