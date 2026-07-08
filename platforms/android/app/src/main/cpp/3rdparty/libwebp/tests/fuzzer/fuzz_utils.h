// Copyright 2018-2024 Google LLC
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

#ifndef WEBP_TESTS_FUZZER_FUZZ_UTILS_H_
#define WEBP_TESTS_FUZZER_FUZZ_UTILS_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "./img_alpha.h"
#include "./img_grid.h"
#include "./img_peak.h"
#include "fuzztest/fuzztest.h"
#include "src/dsp/cpu.h"
#include "src/webp/decode.h"
#include "src/webp/encode.h"
#include "src/webp/types.h"

namespace fuzz_utils {

//------------------------------------------------------------------------------
// Arbitrary limits to prevent OOM, timeout, or slow execution.

// The decoded image size, and for animations additionally the canvas size.
// Enabling some sanitizers slow down runtime significantly.
// Use a very low threshold in this case to avoid timeouts.
#if defined(__SANITIZE_ADDRESS__)  // GCC
static const size_t kFuzzPxLimit = 1024 * 1024 / 10;
#elif !defined(__has_feature)  // Clang
static const size_t kFuzzPxLimit = 1024 * 1024;
#elif __has_feature(address_sanitizer) || __has_feature(memory_sanitizer)
static const size_t kFuzzPxLimit = 1024 * 1024 / 18;
#else
static const size_t kFuzzPxLimit = 1024 * 1024;
#endif

// Demuxed or decoded animation frames.
static const int kFuzzFrameLimit = 3;

// Reads and sums (up to) 128 spread-out bytes.
static WEBP_INLINE uint8_t FuzzHash(const uint8_t* const data, size_t size) {
  uint8_t value = 0;
  size_t incr = size / 128;
  if (!incr) incr = 1;
  for (size_t i = 0; i < size; i += incr) value += data[i];
  return value;
}

#ifdef __cplusplus
extern "C" VP8CPUInfo VP8GetCPUInfo;
#else
extern VP8CPUInfo VP8GetCPUInfo;
#endif

//------------------------------------------------------------------------------

constexpr const uint8_t* kImagesData[] = {kImgAlphaData, kImgGridData,
                                          kImgPeakData};
constexpr size_t kNumSourceImages =
    sizeof(kImagesData) / sizeof(kImagesData[0]);

WebPPicture GetSourcePicture(int image_index, bool use_argb);

// Struct to use in a unique_ptr to free the memory.
struct UniquePtrDeleter {
  void operator()(WebPMemoryWriter* writer) const {
    WebPMemoryWriterClear(writer);
  }
  void operator()(WebPPicture* pic) const { WebPPictureFree(pic); }
  void operator()(WebPDecoderConfig* config) const {
    WebPFreeDecBuffer(&config->output);
  }
};

// Like WebPPicture but with no C array.
// This can be removed once b/294098900 is fixed.
struct WebPPictureCpp {
  inline WebPPictureCpp(int use_argb, WebPEncCSP colorspace, int width,
                        int height, uint8_t* y, uint8_t* u, uint8_t* v,
                        int y_stride, int uv_stride, uint8_t* a, int a_stride,
                        uint32_t* argb, int argb_stride, void* memory,
                        void* memory_argb) {
    pic.reset(new WebPPicture(), [](WebPPicture* pic) {
      WebPPictureFree(pic);
      delete pic;
    });
    if (!WebPPictureInit(pic.get())) assert(false);
    pic->use_argb = use_argb;
    pic->colorspace = colorspace;
    pic->width = width;
    pic->height = height;
    pic->y = y;
    pic->u = u;
    pic->v = v;
    pic->a = a;
    pic->y_stride = y_stride;
    pic->uv_stride = uv_stride;
    pic->a_stride = a_stride;
    pic->argb = argb;
    pic->argb_stride = argb_stride;
    pic->memory_ = memory;
    pic->memory_argb_ = memory_argb;
  }
  WebPPicture& ref() const { return const_cast<WebPPicture&>(*pic); }
  std::shared_ptr<WebPPicture> pic;
};

static inline auto ArbitraryWebPPicture() {
  return fuzztest::FlatMap(
      // colorspace of 0 is use_argb, 1 is YUV420, 2 is YUV420A.
      [](int colorspace, int width, int height) {
        const int uv_width = (int)(((int64_t)width + 1) >> 1);
        const int uv_height = (int)(((int64_t)height + 1) >> 1);
        // Create a domain for the vector that strictly obeys w * h * 4.
        size_t size = width * height;
        if (colorspace == 0) size *= 4;
        if (colorspace == 1) size += 2 * uv_width * uv_height;
        if (colorspace == 2) size += 2 * uv_width * uv_height + size;
        auto DataDomain =
            fuzztest::VectorOf(fuzztest::Arbitrary<uint8_t>()).WithSize(size);

        // Map the vector domain back into our Image struct, injecting w and h
        return fuzztest::Map(
            [colorspace, width,
             height](const std::vector<uint8_t>& data) -> WebPPictureCpp {
              WebPPicture pic;
              if (!WebPPictureInit(&pic)) assert(false);
              pic.use_argb = colorspace == 0 ? 1 : 0;
              pic.colorspace = static_cast<WebPEncCSP>(
                  colorspace <= 1 ? WEBP_YUV420 : WEBP_YUV420A);
              pic.width = width;
              pic.height = height;
              if (!WebPPictureAlloc(&pic)) assert(false);
              size_t size = width * height;
              if (pic.use_argb) {
                std::copy(data.begin(), data.begin() + size,
                          (uint32_t*)pic.argb);
              } else {
                // Y.
                auto iter = data.begin();
                std::copy(iter, iter + size, (uint8_t*)pic.y);
                iter += size;
                // A.
                if ((int)pic.colorspace & WEBP_CSP_ALPHA_BIT) {
                  std::copy(iter, iter + size, (uint8_t*)pic.a);
                  iter += size;
                }
                // U and V.
                const int uv_width = (int)(((int64_t)width + 1) >> 1);
                const int uv_height = (int)(((int64_t)height + 1) >> 1);
                size = uv_width * uv_height;
                std::copy(iter, iter + size, (uint8_t*)pic.u);
                iter += size;
                std::copy(iter, iter + size, (uint8_t*)pic.v);
              }
              return WebPPictureCpp(pic.use_argb, pic.colorspace, pic.width,
                                    pic.height, pic.y, pic.u, pic.v,
                                    pic.y_stride, pic.uv_stride, pic.a,
                                    pic.a_stride, pic.argb, pic.argb_stride,
                                    pic.memory_, pic.memory_argb_);
            },
            DataDomain);
      },
      /*colorspace=*/fuzztest::InRange<int>(0, 2),
      /*width=*/fuzztest::InRange<int>(1, 128),
      /*height=*/fuzztest::InRange<int>(1, 128));
}

static inline auto ArbitraryWebPPictureFromIndex() {
  return fuzztest::Map(
      [](int index, bool use_argb) -> WebPPictureCpp {
        // Map the vector domain back into our Image struct, injecting w and h
        WebPPicture pic = fuzz_utils::GetSourcePicture(index, use_argb);
        return WebPPictureCpp(use_argb, pic.colorspace, pic.width, pic.height,
                              pic.y, pic.u, pic.v, pic.y_stride, pic.uv_stride,
                              pic.a, pic.a_stride, pic.argb, pic.argb_stride,
                              pic.memory_, pic.memory_argb_);
      },
      /*index=*/fuzztest::InRange<int>(0, fuzz_utils::kNumSourceImages - 1),
      /*use_argb=*/fuzztest::Arbitrary<bool>());
}

static inline auto ArbitraryWebPConfig() {
  return fuzztest::Map(
      [](int lossless, int quality, int method, int image_hint, int segments,
         int sns_strength, int filter_strength, int filter_sharpness,
         int filter_type, int autofilter, int alpha_compression,
         int alpha_filtering, int alpha_quality, int pass, int preprocessing,
         int partitions, int partition_limit, int emulate_jpeg_size,
         int thread_level, int low_memory, int near_lossless, int exact,
         int use_delta_palette, int use_sharp_yuv) -> WebPConfig {
        WebPConfig config;
        if (!WebPConfigInit(&config)) assert(false);
        config.lossless = lossless;
        config.quality = quality;
        config.method = method;
        config.image_hint = (WebPImageHint)image_hint;
        config.segments = segments;
        config.sns_strength = sns_strength;
        config.filter_strength = filter_strength;
        config.filter_sharpness = filter_sharpness;
        config.filter_type = filter_type;
        config.autofilter = autofilter;
        config.alpha_compression = alpha_compression;
        config.alpha_filtering = alpha_filtering;
        config.alpha_quality = alpha_quality;
        config.pass = pass;
        config.show_compressed = 1;
        config.preprocessing = preprocessing;
        config.partitions = partitions;
        config.partition_limit = 10 * partition_limit;
        config.emulate_jpeg_size = emulate_jpeg_size;
        config.thread_level = thread_level;
        config.low_memory = low_memory;
        config.near_lossless = 20 * near_lossless;
        config.exact = exact;
        config.use_delta_palette = use_delta_palette;
        config.use_sharp_yuv = use_sharp_yuv;
        if (!WebPValidateConfig(&config)) assert(false);
        return config;
      },
      /*lossless=*/fuzztest::InRange<int>(0, 1),
      /*quality=*/fuzztest::InRange<int>(0, 100),
      /*method=*/fuzztest::InRange<int>(0, 6),
      /*image_hint=*/fuzztest::InRange<int>(0, WEBP_HINT_LAST - 1),
      /*segments=*/fuzztest::InRange<int>(1, 4),
      /*sns_strength=*/fuzztest::InRange<int>(0, 100),
      /*filter_strength=*/fuzztest::InRange<int>(0, 100),
      /*filter_sharpness=*/fuzztest::InRange<int>(0, 7),
      /*filter_type=*/fuzztest::InRange<int>(0, 1),
      /*autofilter=*/fuzztest::InRange<int>(0, 1),
      /*alpha_compression=*/fuzztest::InRange<int>(0, 1),
      /*alpha_filtering=*/fuzztest::InRange<int>(0, 2),
      /*alpha_quality=*/fuzztest::InRange<int>(0, 100),
      /*pass=*/fuzztest::InRange<int>(1, 10),
      /*preprocessing=*/fuzztest::InRange<int>(0, 2),
      /*partitions=*/fuzztest::InRange<int>(0, 3),
      /*partition_limit=*/fuzztest::InRange<int>(0, 10),
      /*emulate_jpeg_size=*/fuzztest::InRange<int>(0, 1),
      /*thread_level=*/fuzztest::InRange<int>(0, 1),
      /*low_memory=*/fuzztest::InRange<int>(0, 1),
      /*near_lossless=*/fuzztest::InRange<int>(0, 5),
      /*exact=*/fuzztest::InRange<int>(0, 1),
      /*use_delta_palette=*/fuzztest::InRange<int>(0, 1),
      /*use_sharp_yuv=*/fuzztest::InRange<int>(0, 1));
}

// Like WebPDecoderOptions but with no C array.
// This can be removed once b/294098900 is fixed.
struct WebPDecoderOptionsCpp {
  int bypass_filtering;
  int no_fancy_upsampling;
  int use_cropping;
  int crop_left, crop_top;

  int crop_width, crop_height;
  int use_scaling;
  int scaled_width, scaled_height;

  int use_threads;
  int dithering_strength;
  int flip;
  int alpha_dithering_strength;

  std::array<uint32_t, 5> pad;
};

static inline auto ArbitraryValidWebPDecoderOptions() {
  return fuzztest::Map(
      [](int bypass_filtering, int no_fancy_upsampling, int use_cropping,
         int crop_left, int crop_top, int crop_width, int crop_height,
         int use_scaling, int scaled_width, int scaled_height, int use_threads,
         int dithering_strength, int flip,
         int alpha_dithering_strength) -> WebPDecoderOptionsCpp {
        WebPDecoderOptions options;
        options.bypass_filtering = bypass_filtering;
        options.no_fancy_upsampling = no_fancy_upsampling;
        options.use_cropping = use_cropping;
        options.crop_left = crop_left;
        options.crop_top = crop_top;
        options.crop_width = crop_width;
        options.crop_height = crop_height;
        options.use_scaling = use_scaling;
        options.scaled_width = scaled_width;
        options.scaled_height = scaled_height;
        options.use_threads = use_threads;
        options.dithering_strength = dithering_strength;
        options.flip = flip;
        options.alpha_dithering_strength = alpha_dithering_strength;
        WebPDecoderConfig config;
        if (!WebPInitDecoderConfig(&config)) assert(false);
        config.options = options;
        if (!WebPValidateDecoderConfig(&config)) assert(false);
        WebPDecoderOptionsCpp options_cpp;
        std::memcpy(&options_cpp, &options, sizeof(options));
        return options_cpp;
      },
      /*bypass_filtering=*/fuzztest::InRange<int>(0, 1),
      /*no_fancy_upsampling=*/fuzztest::InRange<int>(0, 1),
      /*use_cropping=*/fuzztest::InRange<int>(0, 1),
      /*crop_left=*/fuzztest::InRange<int>(0, 10),
      /*crop_top=*/fuzztest::InRange<int>(0, 10),
      /*crop_width=*/fuzztest::InRange<int>(1, 10),
      /*crop_height=*/fuzztest::InRange<int>(1, 10),
      /*use_scaling=*/fuzztest::InRange<int>(0, 1),
      /*scaled_width=*/fuzztest::InRange<int>(1, 10),
      /*scaled_height=*/fuzztest::InRange<int>(1, 10),
      /*use_threads=*/fuzztest::InRange<int>(0, 1),
      /*dithering_strength=*/fuzztest::InRange<int>(0, 100),
      /*flip=*/fuzztest::InRange<int>(0, 1),
      /*alpha_dithering_strength=*/fuzztest::InRange<int>(0, 100));
}

static inline auto ArbitraryWebPDecoderOptions() {
  return fuzztest::Map(
      [](int bypass_filtering, int no_fancy_upsampling, int use_cropping,
         int crop_left, int crop_top, int crop_width, int crop_height,
         int use_scaling, int scaled_width, int scaled_height, int use_threads,
         int dithering_strength, int flip,
         int alpha_dithering_strength) -> WebPDecoderOptionsCpp {
        WebPDecoderOptions options;
        options.bypass_filtering = bypass_filtering;
        options.no_fancy_upsampling = no_fancy_upsampling;
        options.use_cropping = use_cropping;
        options.crop_left = crop_left;
        options.crop_top = crop_top;
        options.crop_width = crop_width;
        options.crop_height = crop_height;
        options.use_scaling = use_scaling;
        options.scaled_width = scaled_width;
        options.scaled_height = scaled_height;
        options.use_threads = use_threads;
        options.dithering_strength = dithering_strength;
        options.flip = flip;
        options.alpha_dithering_strength = alpha_dithering_strength;
        WebPDecoderOptionsCpp options_cpp;
        std::memcpy(&options_cpp, &options, sizeof(options));
        return options_cpp;
      },
      /*bypass_filtering=*/fuzztest::Arbitrary<int>(),
      /*no_fancy_upsampling=*/fuzztest::Arbitrary<int>(),
      /*use_cropping=*/fuzztest::Arbitrary<int>(),
      /*crop_left=*/fuzztest::Arbitrary<int>(),
      /*crop_top=*/fuzztest::Arbitrary<int>(),
      /*crop_width=*/fuzztest::Arbitrary<int>(),
      /*crop_height=*/fuzztest::Arbitrary<int>(),
      /*use_scaling=*/fuzztest::Arbitrary<int>(),
      /*scaled_width=*/fuzztest::Arbitrary<int>(),
      /*scaled_height=*/fuzztest::Arbitrary<int>(),
      /*use_threads=*/fuzztest::Arbitrary<int>(),
      /*dithering_strength=*/fuzztest::Arbitrary<int>(),
      /*flip=*/fuzztest::Arbitrary<int>(),
      /*alpha_dithering_strength=*/fuzztest::Arbitrary<int>());
}

struct CropOrScaleParams {
  bool alter_input;
  bool crop_or_scale;
  int width_ratio;
  int height_ratio;
  int left_ratio;
  int top_ratio;
};

static inline auto ArbitraryCropOrScaleParams() {
  return fuzztest::Map(
      [](const std::optional<std::pair<int, int>>& width_height_ratio,
         const std::optional<std::pair<int, int>>& left_top_ratio)
          -> CropOrScaleParams {
        CropOrScaleParams params;
        params.alter_input = width_height_ratio.has_value();
        if (params.alter_input) {
          params.width_ratio = width_height_ratio->first;
          params.height_ratio = width_height_ratio->second;
          params.crop_or_scale = left_top_ratio.has_value();
          if (params.crop_or_scale) {
            params.left_ratio = left_top_ratio->first;
            params.top_ratio = left_top_ratio->second;
          }
        }
        return params;
      },
      fuzztest::OptionalOf(
          fuzztest::PairOf(fuzztest::InRange(1, 8), fuzztest::InRange(1, 8))),
      fuzztest::OptionalOf(
          fuzztest::PairOf(fuzztest::InRange(1, 8), fuzztest::InRange(1, 8))));
}

// Crops or scales a picture according to the given params.
int CropOrScale(WebPPicture* pic, const CropOrScaleParams& params);

// Imposes a level of optimization among one of the kMaxOptimizationIndex+1
// possible values: OnlyC, ForceSlowSSSE3, NoSSE41, NoAVX, default.
static constexpr uint32_t kMaxOptimizationIndex = 4;
void SetOptimization(VP8CPUInfo default_VP8GetCPUInfo, uint32_t index);

//------------------------------------------------------------------------------

// See https://developers.google.com/speed/webp/docs/riff_container.
static constexpr size_t kMaxWebPFileSize = (1ull << 32) - 2;  // 4 GiB - 2

std::vector<std::string> GetDictionaryFromFiles(
    const std::vector<std::string_view>& file_paths);

// Checks whether the binary blob containing a JPEG or WebP is too big for the
// fuzzer.
bool IsImageTooBig(const uint8_t* data, size_t size);

}  // namespace fuzz_utils

#endif  // WEBP_TESTS_FUZZER_FUZZ_UTILS_H_
