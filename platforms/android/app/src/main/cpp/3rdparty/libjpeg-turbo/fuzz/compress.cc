/*
 * Copyright (C) 2021, 2023-2026 D. R. Commander
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

extern "C" unsigned char *
_tj3LoadImageFromFileHandle8(tjhandle handle, FILE *file, int *width,
                             int align, int *height, int *pixelFormat);


#define NUMTESTS  7


struct test {
  int bottomUp;
  enum TJPF pf;
  int colorspace;
  enum TJSAMP subsamp;
  int fastDCT, quality, optimize, progressive, arithmetic, noRealloc,
    restartRows;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  tjhandle handle = NULL;
  unsigned char *imgBuf = NULL, *srcBuf, *dstBuf = NULL;
  int width = 0, height = 0, ti;
  FILE *file = NULL;
  struct test tests[NUMTESTS] = {
    /*
      BU Pixel      JPEG          Subsampling  Fst Qual Opt Prg Ari No    Rst
         Format     Colorspace    Level        DCT                  Realc Rows */
    { 1, TJPF_RGB,  TJCS_RGB,     TJSAMP_444,  0,  100, 0,  0,  0,  0,    2    },
    { 0, TJPF_BGR,  TJCS_YCbCr,   TJSAMP_422,  0,  90,  0,  1,  0,  0,    0    },
    { 0, TJPF_RGBX, TJCS_DEFAULT, TJSAMP_420,  1,  75,  0,  0,  1,  1,    0    },
    { 0, TJPF_BGRA, TJCS_DEFAULT, TJSAMP_411,  0,  50,  0,  1,  1,  0,    0    },
    { 0, TJPF_XRGB, TJCS_GRAY,    TJSAMP_GRAY, 0,  25,  0,  0,  0,  0,    0    },
    { 0, TJPF_GRAY, TJCS_DEFAULT, TJSAMP_GRAY, 0,  10,  0,  0,  0,  0,    0    },
    { 0, TJPF_CMYK, TJCS_YCCK,    TJSAMP_440,  0,  1,   1,  0,  0,  0,    2    }
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
    tj3Set(handle, TJPARAM_COLORSPACE, tests[ti].colorspace);
    tj3Set(handle, TJPARAM_FASTDCT, tests[ti].fastDCT);
    tj3Set(handle, TJPARAM_OPTIMIZE, tests[ti].optimize);
    tj3Set(handle, TJPARAM_PROGRESSIVE, tests[ti].progressive);
    tj3Set(handle, TJPARAM_ARITHMETIC, tests[ti].arithmetic);
    tj3Set(handle, TJPARAM_NOREALLOC, tests[ti].noRealloc);
    tj3Set(handle, TJPARAM_RESTARTROWS, tests[ti].restartRows);

    tj3Set(handle, TJPARAM_MAXPIXELS, 1048576);
    /* tj3LoadImage8() will refuse to load images larger than 1 Megapixel, so
       we don't need to check the width and height here. */
    fseek(file, 0, SEEK_SET);
    if ((imgBuf = _tj3LoadImageFromFileHandle8(handle, file, &width, 1,
                                               &height, &pf)) == NULL) {
      if (size < 2)
        continue;

      /* Derive image dimensions from input data.  Use first 2 bytes to
         influence width/height. */
      width = (data[0] % 64) + 8;   /* 8-71 */
      height = (data[1] % 64) + 8;  /* 8-71 */

      size_t required_size = 2 + (size_t)width * height *
                             tjPixelSize[tests[ti].pf];
      if (size < required_size) {
        /* Not enough data - try smaller dimensions */
        width = 8;
        height = 8;
        required_size = 2 + (size_t)width * height *
                        tjPixelSize[tests[ti].pf];
        if (size < required_size)
          continue;
      }

      /* Skip header bytes. */
      srcBuf = (unsigned char *)data + 2;
    } else
      srcBuf = imgBuf;

    dstSize = maxBufSize = tj3JPEGBufSize(width, height, tests[ti].subsamp);
    if (tj3Get(handle, TJPARAM_NOREALLOC)) {
      if ((dstBuf = (unsigned char *)tj3Alloc(dstSize)) == NULL)
        goto bailout;
    } else if (dstBuf == NULL) {
      if ((dstBuf = (unsigned char *)tj3Alloc(100)) == NULL)
        goto bailout;
      dstSize = 100;
    }

    if (size >= 34)
      tj3SetICCProfile(handle, (unsigned char *)&data[2], 32);

    tj3Set(handle, TJPARAM_SUBSAMP, tests[ti].subsamp);
    tj3Set(handle, TJPARAM_QUALITY, tests[ti].quality);
    if (tj3Compress8(handle, srcBuf, width, 0, height, pf, &dstBuf,
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
