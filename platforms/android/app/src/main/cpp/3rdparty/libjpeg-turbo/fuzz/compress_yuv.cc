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

extern "C" unsigned char *
_tj3LoadImageFromFileHandle8(tjhandle handle, FILE *file, int *width,
                             int align, int *height, int *pixelFormat);


#define NUMTESTS  6


struct test {
  int bottomUp;
  enum TJPF pf;
  enum TJSAMP subsamp;
  int fastDCT, quality, optimize, progressive, arithmetic, restartBlocks;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  tjhandle handle = NULL;
  unsigned char *imgBuf = NULL, *srcBuf, *dstBuf = NULL, *yuvBuf = NULL;
  int width = 0, height = 0, ti;
  FILE *file = NULL;
  struct test tests[NUMTESTS] = {
    /*
      BU Pixel      Subsampling  Fst Qual Opt Prg Ari Rst
         Format     Level        DCT                  Blks */
    { 0, TJPF_XBGR, TJSAMP_444,  1,  100, 0,  0,  0,  0    },
    { 0, TJPF_XRGB, TJSAMP_422,  0,  90,  0,  1,  0,  4    },
    { 0, TJPF_BGR,  TJSAMP_420,  0,  75,  0,  0,  0,  0    },
    { 0, TJPF_RGB,  TJSAMP_411,  0,  50,  1,  0,  0,  0    },
    { 0, TJPF_BGR,  TJSAMP_GRAY, 0,  25,  0,  0,  1,  0    },
    { 1, TJPF_GRAY, TJSAMP_GRAY, 1,  10,  0,  1,  1,  4    }
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
    tj3Set(handle, TJPARAM_FASTDCT, tests[ti].fastDCT);
    tj3Set(handle, TJPARAM_OPTIMIZE, tests[ti].optimize);
    tj3Set(handle, TJPARAM_PROGRESSIVE, tests[ti].progressive);
    tj3Set(handle, TJPARAM_ARITHMETIC, tests[ti].arithmetic);
    tj3Set(handle, TJPARAM_NOREALLOC, 1);
    tj3Set(handle, TJPARAM_RESTARTBLOCKS, tests[ti].restartBlocks);

    tj3Set(handle, TJPARAM_MAXPIXELS, 1048576);
    /* tj3LoadImage8() will refuse to load images larger than 1 Megapixel, so
       we don't need to check the width and height here. */
    fseek(file, 0, SEEK_SET);
    if ((imgBuf = _tj3LoadImageFromFileHandle8(handle, file, &width, 1,
                                               &height, &pf)) == NULL) {
      if (size < 2)
        continue;

      /* Derive image dimensions from input data.  Use first 2 bytes to
         influence width/height.  These must be multiples of the maximum iMCU
         size for the subsampling levels we plan to test. */
      width = ((data[0] % 4) + 1) * 32;   /* 32-128, multiple of 32 */
      height = ((data[1] % 8) + 1) * 16;  /* 16-128, multiple of 16 */

      size_t required_size = 2 + (size_t)width * height *
                             tjPixelSize[tests[ti].pf];
      if (size < required_size) {
        /* Not enough data - try smaller dimensions */
        width = 32;
        height = 16;
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
    if ((dstBuf = (unsigned char *)tj3Alloc(dstSize)) == NULL)
      goto bailout;
    if ((yuvBuf =
         (unsigned char *)malloc(tj3YUVBufSize(width, 1, height,
                                               tests[ti].subsamp))) == NULL)
      goto bailout;

    tj3Set(handle, TJPARAM_SUBSAMP, tests[ti].subsamp);
    tj3Set(handle, TJPARAM_QUALITY, tests[ti].quality);
    if (tj3EncodeYUV8(handle, srcBuf, width, 0, height, pf, yuvBuf, 1) == 0 &&
        tj3CompressFromYUV8(handle, yuvBuf, width, 1, height, &dstBuf,
                            &dstSize) == 0) {
      /* Touch all of the output data in order to catch uninitialized reads
         when using MemorySanitizer. */
      for (i = 0; i < dstSize; i++)
        sum += dstBuf[i];
    }

    tj3Free(dstBuf);
    dstBuf = NULL;
    free(yuvBuf);
    yuvBuf = NULL;
    tj3Free(imgBuf);
    imgBuf = NULL;

    /* Prevent the sum above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > 255 * maxBufSize)
      goto bailout;
  }

bailout:
  tj3Free(dstBuf);
  free(yuvBuf);
  tj3Free(imgBuf);
  if (file) fclose(file);
  tj3Destroy(handle);
  return 0;
}
