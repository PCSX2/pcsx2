// Copyright 2018 Google Inc.
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

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "./fuzz_utils.h"
#include "src/dsp/cpu.h"
#include "src/utils/rescaler_utils.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace {

const VP8CPUInfo default_VP8GetCPUInfo = fuzz_utils::VP8GetCPUInfo;

void Enc(const fuzz_utils::CropOrScaleParams& crop_or_scale_params,
         WebPConfig& config, WebPPicture& pic,
         WebPMemoryWriter& memory_writer) {
  // Crop and scale.
  if (!fuzz_utils::CropOrScale(&pic, crop_or_scale_params)) {
    const WebPEncodingError error_code = pic.error_code;
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return;
    fprintf(stderr, "ExtractAndCropOrScale failed. Error code: %d\n",
            error_code);
    std::abort();
  }

  // Skip slow settings on big images, it's likely to timeout.
  if (pic.width * pic.height > 32 * 32) {
    if (config.lossless) {
      if (config.quality > 99.0f && config.method >= 5) {
        config.quality = 99.0f;
        config.method = 5;
      }
    } else {
      if (config.quality > 99.0f && config.method == 6) {
        config.quality = 99.0f;
      }
    }
    if (config.alpha_quality == 100 && config.method == 6) {
      config.alpha_quality = 99;
    }
  }

  // Encode.
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &memory_writer;
  if (!WebPEncode(&config, &pic)) {
    const WebPEncodingError error_code = pic.error_code;
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY ||
        error_code == VP8_ENC_ERROR_BAD_WRITE) {
      return;
    }
    fprintf(stderr, "WebPEncode failed. Error code: %d\n", error_code);
    std::abort();
  }
}

void EncDecValidTest(bool use_argb, fuzz_utils::WebPPictureCpp pic_cpp,
                     WebPConfig config, int optimization_index,
                     const fuzz_utils::CropOrScaleParams& crop_or_scale_params,
                     int colorspace,
                     const fuzz_utils::WebPDecoderOptionsCpp& decoder_options) {
  fuzz_utils::SetOptimization(default_VP8GetCPUInfo, optimization_index);

  // Init the source picture.
  WebPPicture& pic = pic_cpp.ref();

  WebPMemoryWriter memory_writer;
  WebPMemoryWriterInit(&memory_writer);
  std::unique_ptr<WebPMemoryWriter, fuzz_utils::UniquePtrDeleter>
      memory_writer_owner(&memory_writer);

  Enc(crop_or_scale_params, config, pic, memory_writer);

  // Try decoding the result.
  const uint8_t* const out_data = memory_writer.mem;
  const size_t out_size = memory_writer.size;
  WebPDecoderConfig dec_config;
  std::unique_ptr<WebPDecoderConfig, fuzz_utils::UniquePtrDeleter>
      dec_config_owner(&dec_config);
  if (!WebPInitDecoderConfig(&dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    std::abort();
  }

  dec_config.output.colorspace = MODE_BGRA;
  VP8StatusCode status = WebPDecode(out_data, out_size, &dec_config);
  if ((status != VP8_STATUS_OK && status != VP8_STATUS_OUT_OF_MEMORY &&
       status != VP8_STATUS_USER_ABORT) ||
      (status == VP8_STATUS_OK && (dec_config.output.width != pic.width ||
                                   dec_config.output.height != pic.height))) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", status);
    std::abort();
  }

  // Compare the results if exact encoding.
  if (status == VP8_STATUS_OK && pic.use_argb && config.lossless &&
      config.near_lossless == 100) {
    const uint8_t* const rgba = dec_config.output.u.RGBA.rgba;
    const int w = dec_config.output.width;
    const int h = dec_config.output.height;

    const uint32_t* src1 = (const uint32_t*)rgba;
    const uint32_t* src2 = pic.argb;
    for (int y = 0; y < h; ++y, src1 += w, src2 += pic.argb_stride) {
      for (int x = 0; x < w; ++x) {
        uint32_t v1 = src1[x], v2 = src2[x];
        if (!config.exact) {
          if ((v1 & 0xff000000u) == 0 || (v2 & 0xff000000u) == 0) {
            // Only keep alpha for comparison of fully transparent area.
            v1 &= 0xff000000u;
            v2 &= 0xff000000u;
          }
        }
        if (v1 != v2) {
          fprintf(stderr, "Lossless compression failed pixel-exactness.\n");
          std::abort();
        }
      }
    }
  }

  // Use given decoding options.
  if (static_cast<int64_t>(decoder_options.crop_left) +
              decoder_options.crop_width >
          static_cast<int64_t>(pic.width) ||
      static_cast<int64_t>(decoder_options.crop_top) +
              decoder_options.crop_height >
          static_cast<int64_t>(pic.height)) {
    return;
  }
  WebPFreeDecBuffer(&dec_config.output);
  if (!WebPInitDecoderConfig(&dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    abort();
  }

  dec_config.output.colorspace = static_cast<WEBP_CSP_MODE>(colorspace);
  std::memcpy(&dec_config.options, &decoder_options, sizeof(decoder_options));
  status = WebPDecode(out_data, out_size, &dec_config);
  if (status != VP8_STATUS_OK && status != VP8_STATUS_OUT_OF_MEMORY &&
      status != VP8_STATUS_USER_ABORT) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", status);
    abort();
  }
}

////////////////////////////////////////////////////////////////////////////////

void EncDecTest(bool use_argb, fuzz_utils::WebPPictureCpp pic_cpp,
                WebPConfig config, int optimization_index,
                const fuzz_utils::CropOrScaleParams& crop_or_scale_params,
                int colorspace,
                const fuzz_utils::WebPDecoderOptionsCpp& decoder_options) {
  fuzz_utils::SetOptimization(default_VP8GetCPUInfo, optimization_index);

  // Init the source picture.
  WebPPicture& pic = pic_cpp.ref();
  WebPMemoryWriter memory_writer;
  WebPMemoryWriterInit(&memory_writer);
  std::unique_ptr<WebPMemoryWriter, fuzz_utils::UniquePtrDeleter>
      memory_writer_owner(&memory_writer);

  Enc(crop_or_scale_params, config, pic, memory_writer);

  // Try decoding the result.
  const uint8_t* const out_data = memory_writer.mem;
  const size_t out_size = memory_writer.size;
  WebPDecoderConfig dec_config;
  std::unique_ptr<WebPDecoderConfig, fuzz_utils::UniquePtrDeleter>
      dec_config_owner(&dec_config);
  if (!WebPInitDecoderConfig(&dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    abort();
  }
  if (decoder_options.use_scaling) {
    int scaled_width = decoder_options.scaled_width;
    int scaled_height = decoder_options.scaled_height;
    if (!WebPRescalerGetScaledDimensions(pic.width, pic.height, &scaled_width,
                                         &scaled_height)) {
      // Rescaled dimensions do not make sense.
      return;
    }
    if (static_cast<uint64_t>(scaled_width) * scaled_height > 1000u * 1000u) {
      // Skip huge scaling.
      return;
    }
  }

  dec_config.output.colorspace = static_cast<WEBP_CSP_MODE>(colorspace);
  std::memcpy(&dec_config.options, &decoder_options, sizeof(decoder_options));
  const VP8StatusCode status = WebPDecode(out_data, out_size, &dec_config);
  if (status != VP8_STATUS_OK && status != VP8_STATUS_OUT_OF_MEMORY &&
      status != VP8_STATUS_USER_ABORT && status != VP8_STATUS_INVALID_PARAM) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", status);
    abort();
  }
}

}  // namespace

FUZZ_TEST(EncIndexDec, EncDecValidTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPictureFromIndex(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::InRange<int>(0, MODE_LAST - 1),
                 fuzz_utils::ArbitraryValidWebPDecoderOptions());

FUZZ_TEST(EncArbitraryDec, EncDecValidTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPicture(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::InRange<int>(0, MODE_LAST - 1),
                 fuzz_utils::ArbitraryValidWebPDecoderOptions());

FUZZ_TEST(EncIndexDec, EncDecTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPictureFromIndex(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::Arbitrary<int>(),
                 fuzz_utils::ArbitraryWebPDecoderOptions());

FUZZ_TEST(EncArbitraryDec, EncDecTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPicture(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::Arbitrary<int>(),
                 fuzz_utils::ArbitraryWebPDecoderOptions());
