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

add_executable(shaderc-online-compile main.cc)
shaderc_default_compile_options(shaderc-online-compile)
target_include_directories(shaderc-online-compile PUBLIC ${shaderc_SOURCE_DIR}/libshaderc_util/include)
target_link_libraries(shaderc-online-compile PRIVATE shaderc)
