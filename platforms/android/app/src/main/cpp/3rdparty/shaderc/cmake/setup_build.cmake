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

if(NOT COMMAND find_host_package)
  macro(find_host_package)
    find_package(${ARGN})
  endmacro()
endif()
if(NOT COMMAND find_host_program)
  macro(find_host_program)
    find_program(${ARGN})
  endmacro()
endif()

# Find asciidoctor; see shaderc_add_asciidoc() from utils.cmake for
# adding documents.
find_program(ASCIIDOCTOR_EXE NAMES asciidoctor)
if (NOT ASCIIDOCTOR_EXE)
  message(STATUS "asciidoctor was not found - no documentation will be"
          " generated")
endif()

# On Windows, CMake by default compiles with the shared CRT.
# Ensure that gmock compiles the same, otherwise failures will occur.
if(WIN32)
  # TODO(awoloszyn): Once we support selecting CRT versions,
  # make sure this matches correctly.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif(WIN32)

if (ANDROID)
# For android let's preemptively find the correct packages so that
# child projects (glslang, googletest) do not fail to find them.

# Tests in glslc and SPIRV-Tools tests require Python 3, or a Python 2
# with the "future" package.  Require Python 3 because we can't force
# developers to manually install the "future" package.
find_host_package(PythonInterp 3 REQUIRED)
find_host_package(BISON)
else()
  find_package(Python COMPONENTS Interpreter REQUIRED)
endif()

option(DISABLE_RTTI "Disable RTTI in builds")
if(DISABLE_RTTI)
  if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fno-rtti")
  endif(UNIX)
endif(DISABLE_RTTI)

option(DISABLE_EXCEPTIONS "Disables exceptions in builds")
if(DISABLE_EXCEPTIONS)
  if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
  endif(UNIX)
endif(DISABLE_EXCEPTIONS)
