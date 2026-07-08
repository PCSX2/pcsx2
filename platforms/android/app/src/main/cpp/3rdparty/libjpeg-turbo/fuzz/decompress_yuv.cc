/*
 * Copyright (C) 2021-2026 D. R. Commander
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../src/turbojpeg.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#define NUMPF  4


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  tjhandle handle = NULL;
  unsigned char *dstBuf = NULL, *yuvBuf = NULL;
  int width = 0, height = 0, jpegSubsamp, pfi;
  /* TJPF_RGB-TJPF_BGR share the same code paths, as do TJPF_RGBX-TJPF_XRGB and
     TJPF_RGBA-TJPF_ARGB.  Thus, the pixel formats below should be the minimum
     necessary to achieve full coverage. */
  enum TJPF pixelFormats[NUMPF] =
    { TJPF_BGR, TJPF_RGBA, TJPF_XRGB, TJPF_GRAY };

  if ((handle = tj3Init(TJINIT_DECOMPRESS)) == NULL)
    goto bailout;

  /* We ignore the return value of tj3DecompressHeader(), because malformed
     JPEG images that might expose issues in libjpeg-turbo might also have
     header errors that cause tj3DecompressHeader() to fail. */
  tj3DecompressHeader(handle, data, size);
  width = tj3Get(handle, TJPARAM_JPEGWIDTH);
  height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
  jpegSubsamp = tj3Get(handle, TJPARAM_SUBSAMP);

  /* Ignore 0-pixel images and images larger than 1 Megapixel.  Casting width
     to (uint64_t) prevents integer overflow if width * height > INT_MAX. */
  if (width < 1 || height < 1 || (uint64_t)width * height > 1048576)
    goto bailout;

  for (pfi = 0; pfi < NUMPF; pfi++) {
    int w = width, h = height;
    int pf = pixelFormats[pfi], i, sum = 0;

    /* Test non-default decompression options on the first iteration. */
    if (!tj3Get(handle, TJPARAM_LOSSLESS)) {
      tj3Set(handle, TJPARAM_BOTTOMUP, pfi == 0);
      tj3Set(handle, TJPARAM_FASTUPSAMPLE, pfi == 0);
      tj3Set(handle, TJPARAM_FASTDCT, pfi == 0);

      /* Test IDCT scaling on the second and third iteration. */
      if (pfi == 1 || pfi == 2) {
        tjscalingfactor sf = { pfi == 1 ? 3 : 1, 4 };
        tj3SetScalingFactor(handle, sf);
        w = TJSCALED(width, sf);
        h = TJSCALED(height, sf);
      } else
        tj3SetScalingFactor(handle, TJUNSCALED);
      tj3Set(handle, TJPARAM_SCANLIMIT, 100);
    } else
      tj3Set(handle, TJPARAM_SCANLIMIT, 50);

    if ((dstBuf = (unsigned char *)tj3Alloc(w * h * tjPixelSize[pf])) == NULL)
      goto bailout;
    if ((yuvBuf =
         (unsigned char *)tj3Alloc(tj3YUVBufSize(w, 1, h,
                                                 jpegSubsamp))) == NULL)
      goto bailout;

    if (tj3DecompressToYUV8(handle, data, size, yuvBuf, 1) == 0 &&
        tj3DecodeYUV8(handle, yuvBuf, 1, dstBuf, w, 0, h, pf) == 0) {
      /* Touch all of the output pixels in order to catch uninitialized reads
         when using MemorySanitizer. */
      for (i = 0; i < w * h * tjPixelSize[pf]; i++)
        sum += dstBuf[i];
    } else if (!strcmp(tj3GetErrorStr(handle),
                       "Progressive JPEG image has more than 100 scans"))
      goto bailout;

    tj3Free(dstBuf);
    dstBuf = NULL;
    tj3Free(yuvBuf);
    yuvBuf = NULL;

    /* Prevent the sum above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > 255 * 1048576 * tjPixelSize[pf])
      goto bailout;
  }

bailout:
  tj3Free(dstBuf);
  tj3Free(yuvBuf);
  tj3Destroy(handle);
  return 0;
}
