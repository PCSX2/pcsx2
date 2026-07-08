// Copyright 2019 The Shaderc Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIBSHADERC_UTIL_INC_ARGS_H
#define LIBSHADERC_UTIL_INC_ARGS_H

#include <cstdint>
#include <string>

#include "libshaderc_util/string_piece.h"

namespace shaderc_util {

// Gets the option argument for the option at *index in argv in a way consistent
// with clang/gcc. On success, returns true and writes the parsed argument into
// *option_argument. Returns false if any errors occur. After calling this
// function, *index will be the index of the last command line argument
// consumed.
bool GetOptionArgument(int argc, char** argv, int* index,
                       const std::string& option,
                       string_piece* option_argument);

// Parses the given string as a number of the specified type.  Returns true
// if parsing succeeded, and stores the parsed value via |value|.
// (I've worked out the general case for this in
// SPIRV-Tools source/util/parse_number.h. -- dneto)
bool ParseUint32(const std::string& str, uint32_t* value);

}  // namespace shaderc_util
#endif  // LIBSHADERC_UTIL_INC_ARGS_H
