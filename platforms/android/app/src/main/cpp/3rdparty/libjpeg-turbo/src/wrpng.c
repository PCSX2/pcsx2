/*
 * wrpng.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2017, 2019-2020, 2022, 2024, 2026, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains routines to write output images in 8-bit-per-channel or
 * 16-bit-per-channel PNG format.  libspng is required to compile this
 * software.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include "cmyk.h"
#include "cdjpeg.h"             /* Common decls for cjpeg/djpeg applications */
#include "spng/spng.h"

#if defined(PNG_SUPPORTED) && \
    (BITS_IN_JSAMPLE != 16 || defined(D_LOSSLESS_SUPPORTED))


#if BITS_IN_JSAMPLE == 8
#define PNG_BIT_DEPTH  8
#define BYTESPERSAMPLE  1
typedef unsigned char PNGSAMPLE;
#else
#define PNG_BIT_DEPTH  16
#define BYTESPERSAMPLE  2
typedef unsigned short PNGSAMPLE;
#endif

#define TRY_SPNG(f) { \
  int __spng_error = (f); \
  if (__spng_error) \
    ERREXITS(cinfo, JERR_PNG_LIBSPNG, spng_strerror(__spng_error)); \
}


/* Private version of data destination object */

typedef struct {
  struct djpeg_dest_struct pub; /* public fields */

  spng_ctx *ctx;
  struct spng_iccp iccp;

  /* Usually these two pointers point to the same place: */
  PNGSAMPLE *iobuffer;          /* libspng's I/O buffer */
  _JSAMPROW pixrow;             /* decompressor output buffer */
  size_t buffer_width;          /* width of I/O buffer */
  PNGSAMPLE *rescale;           /* data precision => PNG bit depth remapping
                                   array, or NULL */
} png_dest_struct;

typedef png_dest_struct *png_dest_ptr;


/*
 * Write some pixel data.
 * In this module rows_supplied will always be 1.
 *
 * put_pixel_rows() is used when the pixel format is JCS_EXT_RGB/JCS_RGB or
 * JCS_GRAYSCALE and cinfo->data_precision is 8 or 16, so we can write the
 * pixels directly to the PNG file.
 */

METHODDEF(void)
put_pixel_rows(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
               JDIMENSION rows_supplied)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  int spng_error;

  spng_error = spng_encode_row(dest->ctx, dest->iobuffer, dest->buffer_width);
  if (spng_error && spng_error != SPNG_EOI)
    ERREXITS(cinfo, JERR_PNG_LIBSPNG, spng_strerror(spng_error)); \
}


/*
 * Convert extended RGB to RGB.
 */

METHODDEF(void)
put_rgb(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo, JDIMENSION rows_supplied)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  register PNGSAMPLE *bufferptr, *rescale = dest->rescale;
  register _JSAMPROW ptr;
  register JDIMENSION col;
  register int rindex = rgb_red[cinfo->out_color_space];
  register int gindex = rgb_green[cinfo->out_color_space];
  register int bindex = rgb_blue[cinfo->out_color_space];
  register int ps = rgb_pixelsize[cinfo->out_color_space];

  ptr = dest->pub._buffer[0];
  bufferptr = dest->iobuffer;
#if BITS_IN_JSAMPLE == PNG_BIT_DEPTH
  if (cinfo->data_precision == PNG_BIT_DEPTH) {
    for (col = cinfo->output_width; col > 0; col--) {
      *bufferptr++ = ptr[rindex];
      *bufferptr++ = ptr[gindex];
      *bufferptr++ = ptr[bindex];
      ptr += ps;
    }
  } else
#endif
  {
    for (col = cinfo->output_width; col > 0; col--) {
      *bufferptr++ = rescale[ptr[rindex]];
      *bufferptr++ = rescale[ptr[gindex]];
      *bufferptr++ = rescale[ptr[bindex]];
      ptr += ps;
    }
  }
  put_pixel_rows(cinfo, dinfo, rows_supplied);
}


/*
 * Convert CMYK to RGB.
 */

METHODDEF(void)
put_cmyk(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
         JDIMENSION rows_supplied)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  register PNGSAMPLE *bufferptr, *rescale = dest->rescale;
  register _JSAMPROW ptr;
  register JDIMENSION col;

  ptr = dest->pub._buffer[0];
  bufferptr = dest->iobuffer;
#if BITS_IN_JSAMPLE == PNG_BIT_DEPTH
  if (cinfo->data_precision == PNG_BIT_DEPTH) {
    for (col = cinfo->output_width; col > 0; col--) {
      _JSAMPLE r, g, b, c = *ptr++, m = *ptr++, y = *ptr++, k = *ptr++;
      cmyk_to_rgb((1 << cinfo->data_precision) - 1, c, m, y, k, &r, &g, &b);
      *bufferptr++ = r;
      *bufferptr++ = g;
      *bufferptr++ = b;
    }
  } else
#endif
  {
    for (col = cinfo->output_width; col > 0; col--) {
      _JSAMPLE r, g, b, c = *ptr++, m = *ptr++, y = *ptr++, k = *ptr++;
      cmyk_to_rgb((1 << cinfo->data_precision) - 1, c, m, y, k, &r, &g, &b);
      *bufferptr++ = rescale[r];
      *bufferptr++ = rescale[g];
      *bufferptr++ = rescale[b];
    }
  }
  put_pixel_rows(cinfo, dinfo, rows_supplied);
}


/*
 * Convert N-bit grayscale to 8-bit or 16-bit grayscale
 */

METHODDEF(void)
put_gray(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
         JDIMENSION rows_supplied)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  register PNGSAMPLE *bufferptr, *rescale = dest->rescale;
  register _JSAMPROW ptr;
  register JDIMENSION col;

  ptr = dest->pub._buffer[0];
  bufferptr = dest->iobuffer;
  for (col = cinfo->output_width; col > 0; col--) {
    *bufferptr++ = rescale[*ptr++];
  }
  put_pixel_rows(cinfo, dinfo, rows_supplied);
}


/*
 * Write some pixel data when color quantization is in effect.
 * We have to demap the color index values to straight data.
 */

METHODDEF(void)
put_demapped_rgb(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
                 JDIMENSION rows_supplied)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  register int pixval;
  register PNGSAMPLE *bufferptr, *rescale = dest->rescale;
  register _JSAMPROW ptr;
  register _JSAMPROW color_map0 =
    ((_JSAMPARRAY)cinfo->colormap)[rgb_red[cinfo->out_color_space]];
  register _JSAMPROW color_map1 =
    ((_JSAMPARRAY)cinfo->colormap)[rgb_green[cinfo->out_color_space]];
  register _JSAMPROW color_map2 =
    ((_JSAMPARRAY)cinfo->colormap)[rgb_blue[cinfo->out_color_space]];
  register JDIMENSION col;

  ptr = dest->pub._buffer[0];
  bufferptr = dest->iobuffer;
#if BITS_IN_JSAMPLE == PNG_BIT_DEPTH
  if (cinfo->data_precision == PNG_BIT_DEPTH) {
    for (col = cinfo->output_width; col > 0; col--) {
      pixval = *ptr++;
      *bufferptr++ = color_map0[pixval];
      *bufferptr++ = color_map1[pixval];
      *bufferptr++ = color_map2[pixval];
    }
  } else
#endif
  {
    for (col = cinfo->output_width; col > 0; col--) {
      pixval = *ptr++;
      *bufferptr++ = rescale[color_map0[pixval]];
      *bufferptr++ = rescale[color_map1[pixval]];
      *bufferptr++ = rescale[color_map2[pixval]];
    }
  }
  put_pixel_rows(cinfo, dinfo, rows_supplied);
}


METHODDEF(void)
put_demapped_gray(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
                  JDIMENSION rows_supplied)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  register PNGSAMPLE *bufferptr, *rescale = dest->rescale;
  register _JSAMPROW ptr;
  register _JSAMPROW color_map = ((_JSAMPARRAY)cinfo->colormap)[0];
  register JDIMENSION col;

  ptr = dest->pub._buffer[0];
  bufferptr = dest->iobuffer;
#if BITS_IN_JSAMPLE == PNG_BIT_DEPTH
  if (cinfo->data_precision == PNG_BIT_DEPTH) {
    for (col = cinfo->output_width; col > 0; col--) {
      *bufferptr++ = color_map[*ptr++];
    }
  } else
#endif
  {
    for (col = cinfo->output_width; col > 0; col--) {
      *bufferptr++ = rescale[color_map[*ptr++]];
    }
  }
  put_pixel_rows(cinfo, dinfo, rows_supplied);
}


/*
 * Embed an ICC profile in the PNG image.
 *
 * NOTE: The pointer passed to this function will be freed in the body of
 * finish_output_png().
 */

METHODDEF(void)
write_icc_profile_png(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
                      const JOCTET *icc_data_ptr, unsigned int icc_data_len)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;

  if (!icc_data_ptr || !icc_data_len)
    ERREXIT(cinfo, JERR_INPUT_EMPTY);

  SNPRINTF(dest->iccp.profile_name, 80, "ICC Profile");
  dest->iccp.profile_len = icc_data_len;
  dest->iccp.profile = (char *)icc_data_ptr;
  TRY_SPNG(spng_set_iccp(dest->ctx, &dest->iccp));
}


/*
 * Startup: write the file header.
 */

METHODDEF(void)
start_output_png(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  struct spng_ihdr ihdr;
  uint8_t color_type = 0;

  switch (cinfo->out_color_space) {
  case JCS_GRAYSCALE:
    color_type = SPNG_COLOR_TYPE_GRAYSCALE;
    break;
  case JCS_RGB:
  case JCS_EXT_RGB:
  case JCS_EXT_RGBX:
  case JCS_EXT_BGR:
  case JCS_EXT_BGRX:
  case JCS_EXT_XBGR:
  case JCS_EXT_XRGB:
  case JCS_EXT_RGBA:
  case JCS_EXT_BGRA:
  case JCS_EXT_ABGR:
  case JCS_EXT_ARGB:
  case JCS_CMYK:
    if (!IsExtRGB(cinfo->out_color_space) && cinfo->quantize_colors)
      ERREXIT(cinfo, JERR_PNG_COLORSPACE);
#if PNG_BIT_DEPTH == 8
    if (cinfo->quantize_colors && cinfo->data_precision == PNG_BIT_DEPTH &&
        IsExtRGB(cinfo->out_color_space))
      color_type = SPNG_COLOR_TYPE_INDEXED;
    else
#endif
      color_type = SPNG_COLOR_TYPE_TRUECOLOR;
    break;
  default:
    ERREXIT(cinfo, JERR_PNG_COLORSPACE);
  }

  TRY_SPNG(spng_set_png_file(dest->ctx, dinfo->output_file));

  memset(&ihdr, 0, sizeof(struct spng_ihdr));
  ihdr.width = (uint32_t)cinfo->output_width;
  ihdr.height = (uint32_t)cinfo->output_height;
  ihdr.bit_depth = PNG_BIT_DEPTH;
  ihdr.color_type = color_type;
  TRY_SPNG(spng_set_ihdr(dest->ctx, &ihdr));

#if PNG_BIT_DEPTH == 8
  if (cinfo->quantize_colors && cinfo->data_precision == PNG_BIT_DEPTH &&
      IsExtRGB(cinfo->out_color_space)) {
    struct spng_plte palette;
    unsigned int i;

    palette.n_entries = cinfo->actual_number_of_colors;
    for (i = 0; i < palette.n_entries; i++) {
      palette.entries[i].red =
        cinfo->colormap[rgb_red[cinfo->out_color_space]][i];
      palette.entries[i].green =
        cinfo->colormap[rgb_green[cinfo->out_color_space]][i];
      palette.entries[i].blue =
        cinfo->colormap[rgb_blue[cinfo->out_color_space]][i];
    }
    TRY_SPNG(spng_set_plte(dest->ctx, &palette));
  }
#endif

  TRY_SPNG(spng_encode_image(dest->ctx, NULL, 0, SPNG_FMT_PNG,
                             SPNG_ENCODE_PROGRESSIVE));
}


/*
 * Finish up at the end of the file.
 */

METHODDEF(void)
finish_output_png(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;

  if (dest->ctx) {
    spng_encode_chunks(dest->ctx);
    free(dest->iccp.profile);
    spng_ctx_free(dest->ctx);
    dest->ctx = NULL;
  }

  /* Make sure we wrote the output file OK */
  fflush(dinfo->output_file);
  if (ferror(dinfo->output_file))
    ERREXIT(cinfo, JERR_FILE_WRITE);
}


/*
 * Re-calculate buffer dimensions based on output dimensions.
 */

METHODDEF(void)
calc_buffer_dimensions_png(j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  png_dest_ptr dest = (png_dest_ptr)dinfo;
  JDIMENSION samples_per_row;

  if (cinfo->out_color_space == JCS_GRAYSCALE)
    samples_per_row = cinfo->output_width * cinfo->out_color_components;
  else
#if PNG_BIT_DEPTH == 8
  if (cinfo->quantize_colors && cinfo->data_precision == PNG_BIT_DEPTH &&
      IsExtRGB(cinfo->out_color_space))
    samples_per_row = cinfo->output_width * cinfo->output_components;
  else
#endif
    samples_per_row = cinfo->output_width * 3;
  dest->buffer_width = samples_per_row * BYTESPERSAMPLE;
}


/*
 * The module selection routine for PNG format output.
 */

GLOBAL(djpeg_dest_ptr)
_jinit_write_png(j_decompress_ptr cinfo)
{
  png_dest_ptr dest;
  boolean use_raw_buffer = FALSE;

#if BITS_IN_JSAMPLE == 8
  if (cinfo->data_precision > BITS_IN_JSAMPLE || cinfo->data_precision < 2)
#else
  if (cinfo->data_precision > BITS_IN_JSAMPLE ||
      cinfo->data_precision < BITS_IN_JSAMPLE - 3)
#endif
    ERREXIT1(cinfo, JERR_BAD_PRECISION, cinfo->data_precision);

  /* Create module interface object, fill in method pointers */
  dest = (png_dest_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                  sizeof(png_dest_struct));
  dest->pub.start_output = start_output_png;
  dest->pub.write_icc_profile = write_icc_profile_png;
  dest->pub.finish_output = finish_output_png;
  dest->pub.calc_buffer_dimensions = calc_buffer_dimensions_png;
  memset(&dest->iccp, 0, sizeof(struct spng_iccp));

  /* Calculate output image dimensions so we can allocate space */
  if (cinfo->image_width > JPEG_MAX_DIMENSION ||
      cinfo->image_height > JPEG_MAX_DIMENSION)
    ERREXIT1(cinfo, JERR_IMAGE_TOO_BIG, JPEG_MAX_DIMENSION);
  jpeg_calc_output_dimensions(cinfo);

  /* Create physical I/O buffer */
  dest->pub.calc_buffer_dimensions(cinfo, (djpeg_dest_ptr)dest);
  dest->iobuffer = (PNGSAMPLE *)(*cinfo->mem->alloc_small)
    ((j_common_ptr)cinfo, JPOOL_IMAGE, dest->buffer_width);

#if PNG_BIT_DEPTH == 8
  if (cinfo->quantize_colors && cinfo->data_precision == PNG_BIT_DEPTH &&
      IsExtRGB(cinfo->out_color_space))
    use_raw_buffer = TRUE;
  else
#endif
  if (!cinfo->quantize_colors && cinfo->data_precision == PNG_BIT_DEPTH &&
      (cinfo->out_color_space == JCS_EXT_RGB ||
#if RGB_RED == 0 && RGB_GREEN == 1 && RGB_BLUE == 2 && RGB_PIXELSIZE == 3
       cinfo->out_color_space == JCS_RGB ||
#endif
       cinfo->out_color_space == JCS_GRAYSCALE))
    use_raw_buffer = TRUE;

  if (use_raw_buffer) {
    /* We will write directly from decompressor output buffer. */
    /* Synthesize a _JSAMPARRAY pointer structure */
    dest->pixrow = (_JSAMPROW)dest->iobuffer;
    dest->pub._buffer = &dest->pixrow;
    dest->pub.buffer_height = 1;
    dest->pub.put_pixel_rows = put_pixel_rows;
  } else {
    /* When quantizing, we need an output buffer for colormap indexes
     * that's separate from the physical I/O buffer.  We also need a
     * separate buffer if pixel format translation must take place.
     */
    dest->pub._buffer = (_JSAMPARRAY)(*cinfo->mem->alloc_sarray)
      ((j_common_ptr)cinfo, JPOOL_IMAGE,
       cinfo->output_width * cinfo->output_components, (JDIMENSION)1);
    dest->pub.buffer_height = 1;
    if (!cinfo->quantize_colors) {
      if (IsExtRGB(cinfo->out_color_space))
        dest->pub.put_pixel_rows = put_rgb;
      else if (cinfo->out_color_space == JCS_CMYK)
        dest->pub.put_pixel_rows = put_cmyk;
      else
        dest->pub.put_pixel_rows = put_gray;
    } else if (cinfo->out_color_space == JCS_GRAYSCALE)
      dest->pub.put_pixel_rows = put_demapped_gray;
    else
      dest->pub.put_pixel_rows = put_demapped_rgb;

    /* Scale up samples with 2-7 or 9-15 bits of data precision so they can be
     * stored in, respectively, 8-bit-per-channel and 16-bit-per-channel PNG
     * files.  This scaling algorithm is fully reversible, i.e. you can get
     * back the same samples with 2-7 or 9-15 bits of data precision, if you
     * similarly scale down the 8-bit or 16-bit samples from the PNG file
     * (which our PNG reader does.)
     */
    if (cinfo->data_precision != PNG_BIT_DEPTH) {
      unsigned int maxval = (1 << cinfo->data_precision) - 1;
      long val, half_maxval = maxval / 2;

      /* Compute the rescaling array. */
      dest->rescale = (PNGSAMPLE *)
        (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                    (size_t)(((long)maxval + 1L) *
                                             sizeof(PNGSAMPLE)));
      memset(dest->rescale, 0, (size_t)(((long)maxval + 1L) *
                                        sizeof(PNGSAMPLE)));
      half_maxval = maxval / 2;
      for (val = 0; val <= (long)maxval; val++) {
        /* The multiplication here must be done in 32 bits to avoid overflow */
        dest->rescale[val] =
          (PNGSAMPLE)((val * ((1 << PNG_BIT_DEPTH) - 1) + half_maxval) /
                      maxval);
      }
    }
  }

  dest->ctx = spng_ctx_new(SPNG_CTX_ENCODER);
  if (!dest->ctx)
    ERREXITS(cinfo, JERR_PNG_LIBSPNG, "Could not create context");

  return (djpeg_dest_ptr)dest;
}

#endif /* defined(PNG_SUPPORTED) &&
          (BITS_IN_JSAMPLE != 16 || defined(D_LOSSLESS_SUPPORTED)) */
