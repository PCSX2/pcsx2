/*
 * Copyright (C) 2021-2024, 2026 D. R. Commander
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

/* This fuzzer uses the libjpeg API to exercise code paths that are not covered
 * by the other fuzzers (or by the TurboJPEG API in general):
 *
 * - JCS_UNKNOWN (NULL color conversion with a component count other than 3 or
 *   4)
 * - Floating point IDCT
 * - Buffered-image mode
 * - Interstitial line skipping
 * - jpeg_save_markers() with a length limit
 * - Custom marker processor
 * - JCS_RGB565
 * - Color quantization
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

extern "C" {
#include "../src/jpeglib.h"
#include "../src/jerror.h"
}


struct fuzzer_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};


static void fuzzer_error_exit(j_common_ptr cinfo)
{
  struct fuzzer_error_mgr *fuzz_err = (struct fuzzer_error_mgr *)cinfo->err;

  longjmp(fuzz_err->setjmp_buffer, 1);
}


static void fuzzer_emit_message(j_common_ptr cinfo, int msg_level)
{
}


static int64_t marker_sum = 0;

static boolean custom_marker_processor(j_decompress_ptr cinfo)
{
  struct jpeg_source_mgr *src = cinfo->src;
  INT32 length;

  /* Read and consume the 2-byte length field. */
  if (src->bytes_in_buffer < 2)
    return FALSE;

  length = ((INT32)src->next_input_byte[0] << 8) +
            (INT32)src->next_input_byte[1];
  src->next_input_byte += 2;
  src->bytes_in_buffer -= 2;
  length -= 2;

  if (length < 0)
    return FALSE;

  /* Consume and touch all marker data in order to catch uninitialized reads
     when using MemorySanitizer. */
  while (length > 0) {
    if (src->bytes_in_buffer == 0) {
      if (!(*src->fill_input_buffer) (cinfo))
        return FALSE;
    }

    size_t available = (size_t)length < src->bytes_in_buffer ?
                       (size_t)length : src->bytes_in_buffer;

    for (size_t i = 0; i < available; i++)
      marker_sum += src->next_input_byte[i];

    src->next_input_byte += available;
    src->bytes_in_buffer -= available;
    length -= (INT32)available;
  }

  return TRUE;
}


#define NUMTESTS  7


struct test {
  J_COLOR_SPACE out_color_space;
  boolean quantize_colors;
  boolean two_pass_quantize;
  J_DITHER_MODE dither_mode;
};


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct jpeg_decompress_struct cinfo;
  struct fuzzer_error_mgr jerr;
  JSAMPARRAY buffer = NULL;
  int row_stride;
  int numTests = 1;
  struct test tests[NUMTESTS] = {
    /*
      Output       Quantize 2-Pass Dither
      Colorspace   Colors   Quant  Mode
    */
    { JCS_RGB565,  FALSE,   FALSE, JDITHER_NONE    },
    { JCS_RGB565,  FALSE,   FALSE, JDITHER_ORDERED },
    { JCS_UNKNOWN, TRUE,    FALSE, JDITHER_NONE    },
    { JCS_UNKNOWN, TRUE,    FALSE, JDITHER_ORDERED },
    { JCS_UNKNOWN, TRUE,    FALSE, JDITHER_FS      },
    { JCS_UNKNOWN, TRUE,    TRUE,  JDITHER_NONE    },
    { JCS_UNKNOWN, TRUE,    TRUE,  JDITHER_FS      }
  };

  /* Reject too-small input. */
  if (size < 2)
    return 0;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = fuzzer_error_exit;
  jerr.pub.emit_message = fuzzer_emit_message;

  jpeg_create_decompress(&cinfo);

  for (int ti = 0; ti < numTests; ti++) {
    int64_t sum = 0;

    marker_sum = 0;

    if (setjmp(jerr.setjmp_buffer)) {
      jpeg_abort_decompress(&cinfo);
      continue;
    }

    jpeg_mem_src(&cinfo, data, (unsigned long)size);

    for (int m = JPEG_APP0; m <= JPEG_APP0 + 15; m++) {
      if (m != JPEG_APP0 + 3)
        jpeg_save_markers(&cinfo, m, 256);
    }
    jpeg_set_marker_processor(&cinfo, JPEG_APP0 + 3, custom_marker_processor);

    jpeg_read_header(&cinfo, TRUE);

    /* Sanity check dimensions to avoid memory exhaustion.  Casting width to
       (uint64_t) prevents integer overflow if width * height > INT_MAX. */
    if (cinfo.image_width < 1 || cinfo.image_height < 1 ||
        (uint64_t)cinfo.image_width * cinfo.image_height > 1048576)
      goto bailout;

    cinfo.dct_method = JDCT_FLOAT;
    cinfo.buffered_image = jpeg_has_multiple_scans(&cinfo);
    if (((cinfo.jpeg_color_space == JCS_YCbCr ||
          cinfo.jpeg_color_space == JCS_RGB) && cinfo.num_components == 3) ||
        (cinfo.jpeg_color_space == JCS_GRAYSCALE &&
         cinfo.num_components == 1)) {
      cinfo.out_color_space = tests[ti].out_color_space;
      if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
        numTests = 5;
        if (cinfo.out_color_space == JCS_UNKNOWN)
          cinfo.out_color_space = JCS_GRAYSCALE;
      } else {
        numTests = 7;
        if (cinfo.out_color_space == JCS_UNKNOWN)
          cinfo.out_color_space = ti % 2 ? JCS_RGB : JCS_EXT_BGR;
      }
      cinfo.quantize_colors = tests[ti].quantize_colors;
      cinfo.two_pass_quantize = tests[ti].two_pass_quantize;
      cinfo.dither_mode = tests[ti].dither_mode;
    }

    if (!jpeg_start_decompress(&cinfo)) {
      jpeg_abort_decompress(&cinfo);
      continue;
    }

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
      ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    if (cinfo.buffered_image) {
      /* Process all scans. */
      while (!jpeg_input_complete(&cinfo) &&
             cinfo.input_scan_number != cinfo.output_scan_number) {
        int retval;

        if (cinfo.input_scan_number > 100) {
          jpeg_abort_decompress(&cinfo);
          goto bailout;
        }

        /* Consume input data until we have a complete scan or reach the end
           of input. */
        do {
          retval = jpeg_consume_input(&cinfo);
        } while (retval != JPEG_SUSPENDED && retval != JPEG_REACHED_SOS &&
                 retval != JPEG_REACHED_EOI);

        if (retval == JPEG_REACHED_EOI)
          break;

        /* Start outputting the current scan. */
        if (!jpeg_start_output(&cinfo, cinfo.input_scan_number))
          break;

        while (cinfo.output_scanline < cinfo.output_height) {
          if (!cinfo.two_pass_quantize &&
              (cinfo.output_scanline == 0 || cinfo.output_scanline == 16)) {
            JDIMENSION output_scanline = cinfo.output_scanline;

            jpeg_skip_scanlines(&cinfo, 8);
            if (cinfo.output_scanline == output_scanline)
              break;
          } else {
            if (jpeg_read_scanlines(&cinfo, buffer, 1) != 1)
              break;
            /* Touch all of the output pixels in order to catch uninitialized
               reads when using MemorySanitizer. */
            for (int i = 0; i < row_stride; i++)
              sum += buffer[0][i];
          }
        }

        /* Finish this output pass. */
        if (!jpeg_finish_output(&cinfo))
          break;
      }

    } else {

      while (cinfo.output_scanline < cinfo.output_height) {
        if (!cinfo.two_pass_quantize &&
            (cinfo.output_scanline == 0 || cinfo.output_scanline == 16))
          jpeg_skip_scanlines(&cinfo, 8);
        else {
          jpeg_read_scanlines(&cinfo, buffer, 1);
          for (int i = 0; i < row_stride; i++)
            sum += buffer[0][i];
        }
      }

    }

    jpeg_finish_decompress(&cinfo);

    /* Prevent the sums above from being optimized out.  This test should never
       be true, but the compiler doesn't know that. */
    if (sum > (int64_t)255 * 1048576 * 4 ||
        marker_sum > (int64_t)255 * 1048576)
      goto bailout;
  }

bailout:
  jpeg_destroy_decompress(&cinfo);
  return 0;
}
