/*
 * Copyright (C) 2021-2026 D. R. Commander
 * Copyright (C) 2025 Leslie P. Polzer
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

extern "C" unsigned short *
_tj3LoadImageFromFileHandle16(tjhandle handle, FILE *file, int *width,
                              int align, int *height, int *pixelFormat);


#define NUMTESTS  7


struct test {
  int bottomUp;
  enum TJPF pf;
  int precision, psv, pt, noRealloc, restartRows;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  tjhandle handle = NULL;
  unsigned short *imgBuf = NULL, *srcBuf;
  unsigned char *dstBuf = NULL;
  int width = 0, height = 0, ti;
  FILE *file = NULL;
  struct test tests[NUMTESTS] = {
    /*
      BU Pixel      Data PSV Pt No    Rst
         Format     Prec        Realc Rows */
    { 1, TJPF_RGB,  16,  1,  0, 1,    1    },
    { 0, TJPF_BGR,  15,  2,  2, 1,    0    },
    { 0, TJPF_RGBX, 14,  3,  4, 0,    0    },
    { 0, TJPF_BGRA, 13,  4,  7, 1,    0    },
    { 0, TJPF_XRGB, 16,  5,  5, 1,    0    },
    { 0, TJPF_GRAY, 16,  6,  3, 1,    0    },
    { 0, TJPF_CMYK, 16,  7,  0, 1,    1    }
  };

  if ((file = fmemopen((void *)data, size, "r")) == NULL)
    goto bailout;

  if ((handle = tj3Init(TJINIT_COMPRESS)) == NULL)
    goto bailout;

  for (ti = 0; ti < NUMTESTS; ti++) {
    int pf = tests[ti].pf;
    size_t dstSize = 0, maxBufSize, i, sum = 0;

    /* Test non-default compression options on specific iterations. */
    tj3Set(handle, TJPARAM_BOTTOMUP, tests[ti].bottomUp);
    tj3Set(handle, TJPARAM_NOREALLOC, tests[ti].noRealloc);
    tj3Set(handle, TJPARAM_PRECISION, tests[ti].precision);
    tj3Set(handle, TJPARAM_RESTARTROWS, tests[ti].restartRows);

    tj3Set(handle, TJPARAM_MAXPIXELS, 1048576);
    /* tj3LoadImage16() will refuse to load images larger than 1 Megapixel, so
       we don't need to check the width and height here. */
    fseek(file, 0, SEEK_SET);
    if ((imgBuf = _tj3LoadImageFromFileHandle16(handle, file, &width, 1,
                                                &height, &pf)) == NULL) {
      if (size < 2)
        continue;

      /* Derive image dimensions from input data.  Use first 2 bytes to
         influence width/height. */
      width = (data[0] % 64) + 8;   /* 8-71 */
      height = (data[1] % 64) + 8;  /* 8-71 */

      size_t required_size = 2 + (size_t)width * height *
                             tjPixelSize[tests[ti].pf] * 2;
      if (size < required_size) {
        /* Not enough data - try smaller dimensions */
        width = 8;
        height = 8;
        required_size = 2 + (size_t)width * height *
                        tjPixelSize[tests[ti].pf] * 2;
        if (size < required_size)
          continue;
      }

      /* Skip header bytes. */
      srcBuf = (unsigned short *)(data + 2);
    } else
      srcBuf = imgBuf;

    dstSize = maxBufSize = tj3JPEGBufSize(width, height, TJSAMP_444);
    if (tj3Get(handle, TJPARAM_NOREALLOC)) {
      if ((dstBuf = (unsigned char *)tj3Alloc(dstSize)) == NULL)
        goto bailout;
    } else
      dstBuf = NULL;

    if (size >= 34)
      tj3SetICCProfile(handle, (unsigned char *)&data[2], 32);

    tj3Set(handle, TJPARAM_LOSSLESS, 1);
    tj3Set(handle, TJPARAM_LOSSLESSPSV, tests[ti].psv);
    tj3Set(handle, TJPARAM_LOSSLESSPT, tests[ti].pt);
    if (tj3Compress16(handle, srcBuf, width, 0, height, pf, &dstBuf,
                      &dstSize) == 0) {
      /* Touch all of the output data in order to catch uninitialized reads
         when using MemorySanitizer. */
      for (i = 0; i < dstSize; i++)
        sum += dstBuf[i];
    }

    tj3Free(dstBuf);
    dstBuf = NULL;
    tj3Free(imgBuf);
    imgBuf = NULL;

    /* Prevent the sum above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > 255 * maxBufSize)
      goto bailout;
  }

bailout:
  tj3Free(dstBuf);
  tj3Free(imgBuf);
  if (file) fclose(file);
  tj3Destroy(handle);
  return 0;
}
