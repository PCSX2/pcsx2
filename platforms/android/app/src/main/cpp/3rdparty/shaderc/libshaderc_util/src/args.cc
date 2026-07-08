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

#include "libshaderc_util/args.h"

#include <iomanip>
#include <sstream>

namespace shaderc_util {

bool GetOptionArgument(int argc, char** argv, int* index,
                       const std::string& option,
                       string_piece* option_argument) {
  const string_piece arg = argv[*index];
  assert(arg.starts_with(option));
  if (arg.size() != option.size()) {
    *option_argument = arg.substr(option.size());
    return true;
  }

  if (option.back() == '=') {
    *option_argument = "";
    return true;
  }

  if (++(*index) >= argc) return false;
  *option_argument = argv[*index];
  return true;
}

bool ParseUint32(const std::string& str, uint32_t* value) {
  std::istringstream iss(str);

  iss >> std::setbase(0);
  iss >> *value;

  // We should have read something.
  bool ok = !str.empty() && !iss.bad();
  // It should have been all the text.
  ok = ok && iss.eof();
  // It should have been in range.
  ok = ok && !iss.fail();

  // Work around a bugs in various C++ standard libraries.
  // Count any negative number as an error, including "-0".
  ok = ok && (str[0] != '-');

  return ok;
}

}  // namespace shaderc_util
