// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// WebPPicture utils for colorspace conversion
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"
#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"
#include "src/dsp/lossless.h"
#include "src/dsp/yuv.h"
#include "src/enc/vp8i_enc.h"
#include "src/utils/random_utils.h"
#include "src/utils/utils.h"
#include "src/webp/encode.h"
#include "src/webp/types.h"

#if defined(WEBP_USE_THREAD) && !defined(_WIN32)
#include <pthread.h>
#endif

#define ALPHA_OFFSET CHANNEL_OFFSET(0)

//------------------------------------------------------------------------------
// Detection of non-trivial transparency

// Returns true if alpha[] has non-0xff values.
static int CheckNonOpaque(const uint8_t* alpha, int width, int height,
                          int x_step, int y_step) {
  if (alpha == NULL) return 0;
  WebPInitAlphaProcessing();
  if (x_step == 1) {
    for (; height-- > 0; alpha += y_step) {
      if (WebPHasAlpha8b(alpha, width)) return 1;
    }
  } else {
    for (; height-- > 0; alpha += y_step) {
      if (WebPHasAlpha32b(alpha, width)) return 1;
    }
  }
  return 0;
}

// Checking for the presence of non-opaque alpha.
int WebPPictureHasTransparency(const WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (picture->use_argb) {
    if (picture->argb != NULL) {
      return CheckNonOpaque((const uint8_t*)picture->argb + ALPHA_OFFSET,
                            picture->width, picture->height, 4,
                            picture->argb_stride * sizeof(*picture->argb));
    }
    return 0;
  }
  return CheckNonOpaque(picture->a, picture->width, picture->height, 1,
                        picture->a_stride);
}

extern VP8CPUInfo VP8GetCPUInfo;

//------------------------------------------------------------------------------
// Sharp RGB->YUV conversion

static const int kMinDimensionIterativeConversion = 4;

//------------------------------------------------------------------------------
// Main function

static int PreprocessARGB(const uint8_t* r_ptr, const uint8_t* g_ptr,
                          const uint8_t* b_ptr, int step, int rgb_stride,
                          WebPPicture* const picture) {
  const int ok = SharpYuvConvert(
      r_ptr, g_ptr, b_ptr, step, rgb_stride, /*rgb_bit_depth=*/8, picture->y,
      picture->y_stride, picture->u, picture->uv_stride, picture->v,
      picture->uv_stride, /*yuv_bit_depth=*/8, picture->width, picture->height,
      SharpYuvGetConversionMatrix(kSharpYuvMatrixWebp));
  if (!ok) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }
  return ok;
}

static WEBP_INLINE void ConvertRowToY(const uint8_t* const r_ptr,
                                      const uint8_t* const g_ptr,
                                      const uint8_t* const b_ptr, int step,
                                      uint8_t* const dst_y, int width,
                                      VP8Random* const rg) {
  int i, j;
  for (i = 0, j = 0; i < width; i += 1, j += step) {
    dst_y[i] =
        VP8RGBToY(r_ptr[j], g_ptr[j], b_ptr[j], VP8RandomBits(rg, YUV_FIX));
  }
}

static WEBP_INLINE void ConvertRowsToUV(const uint16_t* rgb,
                                        uint8_t* const dst_u,
                                        uint8_t* const dst_v, int width,
                                        VP8Random* const rg) {
  int i;
  for (i = 0; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    dst_u[i] = VP8RGBToU(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
    dst_v[i] = VP8RGBToV(r, g, b, VP8RandomBits(rg, YUV_FIX + 2));
  }
}

extern void SharpYuvInit(VP8CPUInfo cpu_info_func);

static int ImportYUVAFromRGBA(const uint8_t* r_ptr, const uint8_t* g_ptr,
                              const uint8_t* b_ptr, const uint8_t* a_ptr,
                              int step,        // bytes per pixel
                              int rgb_stride,  // bytes per scanline
                              float dithering, int use_iterative_conversion,
                              WebPPicture* const picture) {
  int y;
  const int width = picture->width;
  const int height = picture->height;
  const int has_alpha = CheckNonOpaque(a_ptr, width, height, step, rgb_stride);

  picture->colorspace = has_alpha ? WEBP_YUV420A : WEBP_YUV420;
  picture->use_argb = 0;

  // disable smart conversion if source is too small (overkill).
  if (width < kMinDimensionIterativeConversion ||
      height < kMinDimensionIterativeConversion) {
    use_iterative_conversion = 0;
  }

  if (!WebPPictureAllocYUVA(picture)) {
    return 0;
  }
  if (has_alpha) {
    assert(step == 4);
  }

  if (use_iterative_conversion) {
    SharpYuvInit(VP8GetCPUInfo);
    if (!PreprocessARGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, picture)) {
      return 0;
    }
    if (has_alpha) {
      WebPExtractAlpha(a_ptr, rgb_stride, width, height, picture->a,
                       picture->a_stride);
    }
  } else {
    const int uv_width = (width + 1) >> 1;
    // temporary storage for accumulated R/G/B values during conversion to U/V
    uint16_t* const tmp_rgb =
        (uint16_t*)WebPSafeMalloc(4 * uv_width, sizeof(*tmp_rgb));
    uint8_t* dst_y = picture->y;
    uint8_t* dst_u = picture->u;
    uint8_t* dst_v = picture->v;
    uint8_t* dst_a = picture->a;

    VP8Random base_rg;
    VP8Random* rg = NULL;
    if (dithering > 0.) {
      VP8InitRandom(&base_rg, dithering);
      rg = &base_rg;
    }
    WebPInitConvertARGBToYUV();
    WebPInitGammaTables();

    if (tmp_rgb == NULL) {
      return WebPEncodingSetError(picture, VP8_ENC_ERROR_OUT_OF_MEMORY);
    }

    if (rg == NULL) {
      // Downsample Y/U/V planes, two rows at a time
      WebPImportYUVAFromRGBA(r_ptr, g_ptr, b_ptr, a_ptr, step, rgb_stride,
                             has_alpha, width, height, tmp_rgb,
                             picture->y_stride, picture->uv_stride,
                             picture->a_stride, dst_y, dst_u, dst_v, dst_a);
      if (height & 1) {
        dst_y += (height - 1) * (ptrdiff_t)picture->y_stride;
        dst_u += (height >> 1) * (ptrdiff_t)picture->uv_stride;
        dst_v += (height >> 1) * (ptrdiff_t)picture->uv_stride;
        r_ptr += (height - 1) * (ptrdiff_t)rgb_stride;
        b_ptr += (height - 1) * (ptrdiff_t)rgb_stride;
        g_ptr += (height - 1) * (ptrdiff_t)rgb_stride;
        if (has_alpha) {
          dst_a += (height - 1) * (ptrdiff_t)picture->a_stride;
          a_ptr += (height - 1) * (ptrdiff_t)rgb_stride;
        }
        WebPImportYUVAFromRGBALastLine(r_ptr, g_ptr, b_ptr, a_ptr, step,
                                       has_alpha, width, tmp_rgb, dst_y, dst_u,
                                       dst_v, dst_a);
      }
    } else {
      // Copy of WebPImportYUVAFromRGBA/WebPImportYUVAFromRGBALastLine,
      // but with dithering.
      for (y = 0; y < (height >> 1); ++y) {
        int rows_have_alpha = has_alpha;
        ConvertRowToY(r_ptr, g_ptr, b_ptr, step, dst_y, width, rg);
        ConvertRowToY(r_ptr + rgb_stride, g_ptr + rgb_stride,
                      b_ptr + rgb_stride, step, dst_y + picture->y_stride,
                      width, rg);
        dst_y += 2 * picture->y_stride;
        if (has_alpha) {
          rows_have_alpha &= !WebPExtractAlpha(a_ptr, rgb_stride, width, 2,
                                               dst_a, picture->a_stride);
          dst_a += 2 * picture->a_stride;
        }
        // Collect averaged R/G/B(/A)
        if (!rows_have_alpha) {
          WebPAccumulateRGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, tmp_rgb,
                            width);
        } else {
          WebPAccumulateRGBA(r_ptr, g_ptr, b_ptr, a_ptr, rgb_stride, tmp_rgb,
                             width);
        }
        // Convert to U/V
        ConvertRowsToUV(tmp_rgb, dst_u, dst_v, uv_width, rg);
        dst_u += picture->uv_stride;
        dst_v += picture->uv_stride;
        r_ptr += 2 * rgb_stride;
        b_ptr += 2 * rgb_stride;
        g_ptr += 2 * rgb_stride;
        if (has_alpha) a_ptr += 2 * rgb_stride;
      }
      if (height & 1) {  // extra last row
        int row_has_alpha = has_alpha;
        ConvertRowToY(r_ptr, g_ptr, b_ptr, step, dst_y, width, rg);
        if (row_has_alpha) {
          row_has_alpha &= !WebPExtractAlpha(a_ptr, 0, width, 1, dst_a, 0);
        }
        // Collect averaged R/G/B(/A)
        if (!row_has_alpha) {
          // Collect averaged R/G/B
          WebPAccumulateRGB(r_ptr, g_ptr, b_ptr, step, /*rgb_stride=*/0,
                            tmp_rgb, width);
        } else {
          WebPAccumulateRGBA(r_ptr, g_ptr, b_ptr, a_ptr, /*rgb_stride=*/0,
                             tmp_rgb, width);
        }
        ConvertRowsToUV(tmp_rgb, dst_u, dst_v, uv_width, rg);
      }
    }

    WebPSafeFree(tmp_rgb);
  }
  return 1;
}

#undef SUM4
#undef SUM2
#undef SUM4ALPHA
#undef SUM2ALPHA

//------------------------------------------------------------------------------
// call for ARGB->YUVA conversion

static int PictureARGBToYUVA(WebPPicture* picture, WebPEncCSP colorspace,
                             float dithering, int use_iterative_conversion) {
  if (picture == NULL) return 0;
  if (picture->argb == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  } else if ((colorspace & WEBP_CSP_UV_MASK) != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  } else {
    const uint8_t* const argb = (const uint8_t*)picture->argb;
    const uint8_t* const a = argb + CHANNEL_OFFSET(0);
    const uint8_t* const r = argb + CHANNEL_OFFSET(1);
    const uint8_t* const g = argb + CHANNEL_OFFSET(2);
    const uint8_t* const b = argb + CHANNEL_OFFSET(3);

    picture->colorspace = WEBP_YUV420;
    return ImportYUVAFromRGBA(r, g, b, a, 4, 4 * picture->argb_stride,
                              dithering, use_iterative_conversion, picture);
  }
}

int WebPPictureARGBToYUVADithered(WebPPicture* picture, WebPEncCSP colorspace,
                                  float dithering) {
  return PictureARGBToYUVA(picture, colorspace, dithering, 0);
}

int WebPPictureARGBToYUVA(WebPPicture* picture, WebPEncCSP colorspace) {
  return PictureARGBToYUVA(picture, colorspace, 0.f, 0);
}

int WebPPictureSharpARGBToYUVA(WebPPicture* picture) {
  return PictureARGBToYUVA(picture, WEBP_YUV420, 0.f, 1);
}
// for backward compatibility
int WebPPictureSmartARGBToYUVA(WebPPicture* picture) {
  return WebPPictureSharpARGBToYUVA(picture);
}

//------------------------------------------------------------------------------
// call for YUVA -> ARGB conversion

int WebPPictureYUVAToARGB(WebPPicture* picture) {
  if (picture == NULL) return 0;
  if (picture->y == NULL || picture->u == NULL || picture->v == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_ALPHA_BIT) && picture->a == NULL) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_NULL_PARAMETER);
  }
  if ((picture->colorspace & WEBP_CSP_UV_MASK) != WEBP_YUV420) {
    return WebPEncodingSetError(picture, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }
  // Allocate a new argb buffer (discarding the previous one).
  if (!WebPPictureAllocARGB(picture)) return 0;
  picture->use_argb = 1;

  // Convert
  {
    int y;
    const int width = picture->width;
    const int height = picture->height;
    const int argb_stride = 4 * picture->argb_stride;
    uint8_t* dst = (uint8_t*)picture->argb;
    const uint8_t *cur_u = picture->u, *cur_v = picture->v, *cur_y = picture->y;
    WebPUpsampleLinePairFunc upsample =
        WebPGetLinePairConverter(ALPHA_OFFSET > 0);

    // First row, with replicated top samples.
    upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    cur_y += picture->y_stride;
    dst += argb_stride;
    // Center rows.
    for (y = 1; y + 1 < height; y += 2) {
      const uint8_t* const top_u = cur_u;
      const uint8_t* const top_v = cur_v;
      cur_u += picture->uv_stride;
      cur_v += picture->uv_stride;
      upsample(cur_y, cur_y + picture->y_stride, top_u, top_v, cur_u, cur_v,
               dst, dst + argb_stride, width);
      cur_y += 2 * picture->y_stride;
      dst += 2 * argb_stride;
    }
    // Last row (if needed), with replicated bottom samples.
    if (height > 1 && !(height & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, width);
    }
    // Insert alpha values if needed, in replacement for the default 0xff ones.
    if (picture->colorspace & WEBP_CSP_ALPHA_BIT) {
      for (y = 0; y < height; ++y) {
        uint32_t* const argb_dst = picture->argb + y * picture->argb_stride;
        const uint8_t* const src = picture->a + y * picture->a_stride;
        int x;
        for (x = 0; x < width; ++x) {
          argb_dst[x] = (argb_dst[x] & 0x00ffffffu) | ((uint32_t)src[x] << 24);
        }
      }
    }
  }
  return 1;
}

//------------------------------------------------------------------------------
// automatic import / conversion

static int Import(WebPPicture* const picture, const uint8_t* rgb,
                  int rgb_stride, int step, int swap_rb, int import_alpha) {
  int y;
  // swap_rb -> b,g,r,a , !swap_rb -> r,g,b,a
  const uint8_t* r_ptr = rgb + (swap_rb ? 2 : 0);
  const uint8_t* g_ptr = rgb + 1;
  const uint8_t* b_ptr = rgb + (swap_rb ? 0 : 2);
  const int width = picture->width;
  const int height = picture->height;

  if (abs(rgb_stride) < (import_alpha ? 4 : 3) * width) return 0;

  if (!picture->use_argb) {
    const uint8_t* a_ptr = import_alpha ? rgb + 3 : NULL;
    return ImportYUVAFromRGBA(r_ptr, g_ptr, b_ptr, a_ptr, step, rgb_stride,
                              0.f /* no dithering */, 0, picture);
  }
  if (!WebPPictureAlloc(picture)) return 0;

  VP8LDspInit();
  WebPInitAlphaProcessing();

  if (import_alpha) {
    // dst[] byte order is {a,r,g,b} for big-endian, {b,g,r,a} for little endian
    uint32_t* dst = picture->argb;
    const int do_copy = (ALPHA_OFFSET == 3) && swap_rb;
    assert(step == 4);
    if (do_copy) {
      for (y = 0; y < height; ++y) {
        memcpy(dst, rgb, width * 4);
        rgb += rgb_stride;
        dst += picture->argb_stride;
      }
    } else {
      for (y = 0; y < height; ++y) {
#ifdef WORDS_BIGENDIAN
        // BGRA or RGBA input order.
        const uint8_t* a_ptr = rgb + 3;
        WebPPackARGB(a_ptr, r_ptr, g_ptr, b_ptr, width, dst);
        r_ptr += rgb_stride;
        g_ptr += rgb_stride;
        b_ptr += rgb_stride;
#else
        // RGBA input order. Need to swap R and B.
        VP8LConvertBGRAToRGBA((const uint32_t*)rgb, width, (uint8_t*)dst);
#endif
        rgb += rgb_stride;
        dst += picture->argb_stride;
      }
    }
  } else {
    uint32_t* dst = picture->argb;
    assert(step >= 3);
    for (y = 0; y < height; ++y) {
      WebPPackRGB(r_ptr, g_ptr, b_ptr, width, step, dst);
      r_ptr += rgb_stride;
      g_ptr += rgb_stride;
      b_ptr += rgb_stride;
      dst += picture->argb_stride;
    }
  }
  return 1;
}

// Public API

#if !defined(WEBP_REDUCE_CSP)

int WebPPictureImportBGR(WebPPicture* picture, const uint8_t* bgr,
                         int bgr_stride) {
  return (picture != NULL && bgr != NULL)
             ? Import(picture, bgr, bgr_stride, 3, 1, 0)
             : 0;
}

int WebPPictureImportBGRA(WebPPicture* picture, const uint8_t* bgra,
                          int bgra_stride) {
  return (picture != NULL && bgra != NULL)
             ? Import(picture, bgra, bgra_stride, 4, 1, 1)
             : 0;
}

int WebPPictureImportBGRX(WebPPicture* picture, const uint8_t* bgrx,
                          int bgrx_stride) {
  return (picture != NULL && bgrx != NULL)
             ? Import(picture, bgrx, bgrx_stride, 4, 1, 0)
             : 0;
}

#endif  // WEBP_REDUCE_CSP

int WebPPictureImportRGB(WebPPicture* picture, const uint8_t* rgb,
                         int rgb_stride) {
  return (picture != NULL && rgb != NULL)
             ? Import(picture, rgb, rgb_stride, 3, 0, 0)
             : 0;
}

int WebPPictureImportRGBA(WebPPicture* picture, const uint8_t* rgba,
                          int rgba_stride) {
  return (picture != NULL && rgba != NULL)
             ? Import(picture, rgba, rgba_stride, 4, 0, 1)
             : 0;
}

int WebPPictureImportRGBX(WebPPicture* picture, const uint8_t* rgbx,
                          int rgbx_stride) {
  return (picture != NULL && rgbx != NULL)
             ? Import(picture, rgbx, rgbx_stride, 4, 0, 0)
             : 0;
}

//------------------------------------------------------------------------------
