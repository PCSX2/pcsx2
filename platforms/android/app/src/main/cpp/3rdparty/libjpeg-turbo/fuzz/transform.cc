/*
 * Copyright (C) 2011, 2021-2026 D. R. Commander
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


static int dummyDCTFilter(short *coeffs, tjregion arrayRegion,
                          tjregion planeRegion, int componentIndex,
                          int transformIndex, tjtransform *transform)
{
  int i;

  for (i = 0; i < arrayRegion.w * arrayRegion.h; i++)
    coeffs[i] = -coeffs[i];
  return 0;
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  tjhandle handle = NULL;
  unsigned char *dstBufs[2] = { NULL, NULL };
  size_t dstSizes[2] = { 0, 0 }, maxBufSize, i;
  int width = 0, height = 0, jpegSubsamp;
  tjtransform transforms[2];

  if ((handle = tj3Init(TJINIT_TRANSFORM)) == NULL)
    goto bailout;

  /* We ignore the return value of tj3DecompressHeader(), because malformed
     JPEG images that might expose issues in libjpeg-turbo might also have
     header errors that cause tj3DecompressHeader() to fail. */
  tj3DecompressHeader(handle, data, size);
  width = tj3Get(handle, TJPARAM_JPEGWIDTH);
  height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
  jpegSubsamp = tj3Get(handle, TJPARAM_SUBSAMP);
  /* Let the transform options dictate the entropy coding algorithm. */
  tj3Set(handle, TJPARAM_ARITHMETIC, 0);
  tj3Set(handle, TJPARAM_PROGRESSIVE, 0);
  tj3Set(handle, TJPARAM_OPTIMIZE, 0);

  /* Ignore 0-pixel images and images larger than 1 Megapixel.  Casting width
     to (uint64_t) prevents integer overflow if width * height > INT_MAX. */
  if (width < 1 || height < 1 || (uint64_t)width * height > 1048576)
    goto bailout;

  tj3Set(handle, TJPARAM_SCANLIMIT, 100);

  if (jpegSubsamp < 0 || jpegSubsamp >= TJ_NUMSAMP)
    jpegSubsamp = TJSAMP_444;

  memset(&transforms[0], 0, sizeof(tjtransform) * 2);

  transforms[0].op = TJXOP_NONE;
  transforms[0].options = TJXOPT_PROGRESSIVE | TJXOPT_COPYNONE;
  dstSizes[0] = maxBufSize = tj3TransformBufSize(handle, &transforms[0]);
  if (dstSizes[0] == 0 ||
      (dstBufs[0] = (unsigned char *)tj3Alloc(dstSizes[0])) == NULL)
    goto bailout;

  if (size >= 34)
    tj3SetICCProfile(handle, (unsigned char *)&data[2], 32);

  tj3Set(handle, TJPARAM_NOREALLOC, 1);
  if (tj3Transform(handle, data, size, 1, dstBufs, dstSizes,
                   transforms) == 0) {
    /* Touch all of the output data in order to catch uninitialized reads when
       using MemorySanitizer. */
    size_t sum = 0;

    for (i = 0; i < dstSizes[0]; i++)
      sum += dstBufs[0][i];

    /* Prevent the sum above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > 255 * maxBufSize)
      goto bailout;
  } else if (!strcmp(tj3GetErrorStr(handle),
                     "Progressive JPEG image has more than 100 scans"))
    goto bailout;


  tj3Free(dstBufs[0]);
  dstBufs[0] = NULL;

  transforms[0].r.w = (height + 1) / 2;
  transforms[0].r.h = (width + 1) / 2;
  transforms[0].op = TJXOP_TRANSPOSE;
  transforms[0].options = TJXOPT_GRAY | TJXOPT_CROP | TJXOPT_COPYNONE |
                          TJXOPT_OPTIMIZE;
  dstSizes[0] = maxBufSize = tj3TransformBufSize(handle, &transforms[0]);
  if (dstSizes[0] == 0 ||
      (dstBufs[0] = (unsigned char *)tj3Alloc(dstSizes[0])) == NULL)
    goto bailout;

  if (tj3Transform(handle, data, size, 1, dstBufs, dstSizes,
                   transforms) == 0) {
    size_t sum = 0;

    for (i = 0; i < dstSizes[0]; i++)
      sum += dstBufs[0][i];

    if (sum > 255 * maxBufSize)
      goto bailout;
  } else if (!strcmp(tj3GetErrorStr(handle),
                     "Progressive JPEG image has more than 100 scans"))
    goto bailout;

  tj3Free(dstBufs[0]);
  dstBufs[0] = NULL;

  transforms[0].op = TJXOP_ROT90;
  transforms[0].options = TJXOPT_TRIM | TJXOPT_ARITHMETIC;
  dstSizes[0] = maxBufSize = tj3TransformBufSize(handle, &transforms[0]);
  if (dstSizes[0] == 0 ||
      (dstBufs[0] = (unsigned char *)tj3Alloc(dstSizes[0])) == NULL)
    goto bailout;

  if (tj3Transform(handle, data, size, 1, dstBufs, dstSizes,
                   transforms) == 0) {
    size_t sum = 0;

    for (i = 0; i < dstSizes[0]; i++)
      sum += dstBufs[0][i];

    if (sum > 255 * maxBufSize)
      goto bailout;
  } else if (!strcmp(tj3GetErrorStr(handle),
                     "Progressive JPEG image has more than 100 scans"))
    goto bailout;

  tj3Free(dstBufs[0]);
  dstBufs[0] = NULL;

  transforms[0].op = TJXOP_NONE;
  transforms[0].options = TJXOPT_PROGRESSIVE;
  transforms[0].customFilter = dummyDCTFilter;
  if ((dstBufs[0] = (unsigned char *)tj3Alloc(100)) == NULL)
    goto bailout;
  dstSizes[0] = 100;

  transforms[1].op = TJXOP_ROT180;
  transforms[1].options = TJXOPT_TRIM;
  if ((dstBufs[1] = (unsigned char *)tj3Alloc(50)) == NULL)
    goto bailout;
  dstSizes[1] = 50;

  maxBufSize = dstSizes[0] + dstSizes[1];

  tj3Set(handle, TJPARAM_NOREALLOC, 0);
  if (tj3Transform(handle, data, size, 2, dstBufs, dstSizes,
                   transforms) == 0) {
    size_t sum = 0;

    for (i = 0; i < dstSizes[0]; i++)
      sum += dstBufs[0][i];
    for (i = 0; i < dstSizes[1]; i++)
      sum += dstBufs[1][i];

    if (sum > 255 * maxBufSize)
      goto bailout;
  } else if (!strcmp(tj3GetErrorStr(handle),
                     "Progressive JPEG image has more than 100 scans"))
    goto bailout;

bailout:
  tj3Free(dstBufs[0]);
  tj3Free(dstBufs[1]);
  tj3Destroy(handle);
  return 0;
}
