// Copyright 2026 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "./fuzz_utils.h"
#include "./nalloc.h"
#include "examples/anim_util.h"

namespace {

void ReadAnimatedImageTest(std::string_view blob) {
  const uint8_t* const data = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();
  if (fuzz_utils::IsImageTooBig(data, size)) return;

  nalloc_init(nullptr);
  nalloc_start(data, size);

  AnimatedImage image;
  if (ReadAnimatedImageFromMemory("random_file", data, size, &image,
                                  /*dump_frames=*/0, /*dump_folder=*/nullptr)) {
    ClearAnimatedImage(&image);
  }

  nalloc_end();
}

}  // namespace

FUZZ_TEST(ReadAnimatedImage, ReadAnimatedImageTest)
    .WithDomains(fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize +
                                                1));
