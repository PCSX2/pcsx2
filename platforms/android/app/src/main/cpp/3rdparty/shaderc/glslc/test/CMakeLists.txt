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

if(${SHADERC_ENABLE_TESTS})

  add_test(NAME shaderc_expect_unittests
          COMMAND ${Python_EXECUTABLE} -m unittest expect_unittest.py
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME shaderc_glslc_test_framework_unittests
          COMMAND ${Python_EXECUTABLE} -m unittest glslc_test_framework_unittest.py
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  # These tests use the glslc executable, so they can only run if
  # glslc has been built.
  if(${SHADERC_ENABLE_EXECUTABLES})
    add_test(NAME glslc_tests
      COMMAND ${Python_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/glslc_test_framework.py
      $<TARGET_FILE:glslc_exe> $<TARGET_FILE:spirv-dis>
      --test-dir ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

endif()
