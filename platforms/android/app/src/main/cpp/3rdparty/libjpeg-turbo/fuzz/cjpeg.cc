/*
 * Copyright (C) 2021, 2024, 2026 D. R. Commander
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

/* This fuzz target wraps cjpeg in order to test esoteric compression options
   as well as the GIF and Targa readers. */

#define CJPEG_FUZZER
extern "C" {
#include "../src/cjpeg.c"
}

#include <stdint.h>
#include <unistd.h>


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  char *argv1[] = {
    (char *)"cjpeg", (char *)"-dct", (char *)"float", (char *)"-memdst",
    (char *)"-quality", (char *)"100,99,98",
    (char *)"-sample", (char *)"4x1,2x2,1x2", (char *)"-targa"
  };
  char *argv2[] = {
    (char *)"cjpeg", (char *)"-dct", (char *)"float", (char *)"-memdst",
    (char *)"-quality", (char *)"90,80,70", (char *)"-smooth", (char *)"50",
    (char *)"-targa"
  };
  FILE *file = NULL;

  if ((file = fmemopen((void *)data, size, "r")) == NULL)
    goto bailout;

  fseek(file, 0, SEEK_SET);
  cjpeg_fuzzer(9, argv1, file);
  fseek(file, 0, SEEK_SET);
  cjpeg_fuzzer(9, argv2, file);

  argv1[8] = argv2[8] = NULL;

  fseek(file, 0, SEEK_SET);
  cjpeg_fuzzer(8, argv1, file);
  fseek(file, 0, SEEK_SET);
  cjpeg_fuzzer(8, argv2, file);

bailout:
  if (file) fclose(file);
  return 0;
}
