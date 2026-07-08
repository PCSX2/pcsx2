/*
 * rdpng.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2015-2017, 2020-2026, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains routines to read input images in 8-bit-per-channel or
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
    (BITS_IN_JSAMPLE != 16 || defined(C_LOSSLESS_SUPPORTED))


static int alpha_index[JPEG_NUMCS] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, 3, 0, 0, -1
};


#define TRY_SPNG(f) { \
  int __spng_error = (f); \
  if (__spng_error) \
    ERREXITS(cinfo, JERR_PNG_LIBSPNG, spng_strerror(__spng_error)); \
}


/* Private version of data source object */

typedef struct {
  struct cjpeg_source_struct pub; /* public fields */

  spng_ctx *ctx;
  uint8_t png_bit_depth, png_color_type;
  int png_alpha;
  struct spng_plte colormap;    /* PNG colormap */

  /* Usually these two pointers point to the same place: */
  unsigned char *iobuffer;      /* libspng's I/O buffer */
  _JSAMPROW pixrow;             /* compressor input buffer */
  size_t buffer_width;          /* width of I/O buffer */
  _JSAMPLE *rescale;            /* PNG bit depth => data precision remapping
                                   array, or NULL */
} png_source_struct;

typedef png_source_struct *png_source_ptr;


/*
 * Read one row of pixels.
 *
 * We provide several different versions depending on input file format.
 * In all cases, input is scaled to cinfo->data_precision.
 *
 * get_raw_row() is used when the pixel format is JCS_EXT_RGB/JCS_RGB or
 * JCS_GRAYSCALE, cinfo->data_precision is 8 or 16, and the PNG file doesn't
 * have an alpha channel.  Thus, we can read the pixels directly from the PNG
 * file.
 */

METHODDEF(JDIMENSION)
get_raw_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  png_source_ptr source = (png_source_ptr)sinfo;
  int spng_error;

  spng_error = spng_decode_row(source->ctx, source->iobuffer,
                               source->buffer_width);
  if (spng_error && spng_error != SPNG_EOI)
    ERREXITS(cinfo, JERR_PNG_LIBSPNG, spng_strerror(spng_error)); \
  return 1;
}


#define GRAY_RGB_READ_LOOP(read_op, alpha_set_op, pointer_op) { \
  for (col = cinfo->image_width; col > 0; col--) { \
    ptr[rindex] = ptr[gindex] = ptr[bindex] = read_op; \
    alpha_set_op \
    ptr += ps; \
    pointer_op \
  } \
}


METHODDEF(JDIMENSION)
get_gray_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit-per-channel or 16-bit-per-channel
 * grayscale or grayscale + alpha PNG files.
 */
{
  png_source_ptr source = (png_source_ptr)sinfo;
  register _JSAMPROW ptr;
  register _JSAMPLE *rescale = source->rescale;
  JDIMENSION col;

  get_raw_row(cinfo, sinfo);
  ptr = source->pub._buffer[0];
#if BITS_IN_JSAMPLE != 12
  if (source->png_bit_depth == cinfo->data_precision) {
    _JSAMPLE *bufferptr = (_JSAMPLE *)source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      *ptr++ = *bufferptr++;
      bufferptr += source->png_alpha;
    }
  } else
#endif
  if (source->png_bit_depth == 16) {
    register unsigned short *bufferptr = (unsigned short *)source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      *ptr++ = rescale[*bufferptr++];
      bufferptr += source->png_alpha;
    }
  } else {
    register unsigned char *bufferptr = source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      *ptr++ = rescale[*bufferptr++];
      bufferptr += source->png_alpha;
    }
  }
  return 1;
}


METHODDEF(JDIMENSION)
get_gray_rgb_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit-per-channel or 16-bit-per-channel
 * grayscale or grayscale + alpha PNG files and converting to extended RGB.
 */
{
  png_source_ptr source = (png_source_ptr)sinfo;
  register _JSAMPROW ptr;
  register _JSAMPLE *rescale = source->rescale;
  JDIMENSION col;
  register int rindex = rgb_red[cinfo->in_color_space];
  register int gindex = rgb_green[cinfo->in_color_space];
  register int bindex = rgb_blue[cinfo->in_color_space];
  register int aindex = alpha_index[cinfo->in_color_space];
  register int ps = rgb_pixelsize[cinfo->in_color_space];

  get_raw_row(cinfo, sinfo);
  ptr = source->pub._buffer[0];
#if BITS_IN_JSAMPLE != 12
  if (source->png_bit_depth == cinfo->data_precision) {
    _JSAMPLE *bufferptr = (_JSAMPLE *)source->iobuffer;
    if (aindex >= 0)
      GRAY_RGB_READ_LOOP(*bufferptr++, ptr[aindex] = _MAXJSAMPLE;,
                         bufferptr += source->png_alpha;)
    else
      GRAY_RGB_READ_LOOP(*bufferptr++, {}, bufferptr += source->png_alpha;)
  } else
#endif
  if (source->png_bit_depth == 16) {
    register unsigned short *bufferptr = (unsigned short *)source->iobuffer;
    if (aindex >= 0)
      GRAY_RGB_READ_LOOP(rescale[*bufferptr++],
                         ptr[aindex] = (1 << cinfo->data_precision) - 1;,
                         bufferptr += source->png_alpha;)
    else
      GRAY_RGB_READ_LOOP(rescale[*bufferptr++], {},
                         bufferptr += source->png_alpha;)
  } else {
    register unsigned char *bufferptr = source->iobuffer;
    if (aindex >= 0)
      GRAY_RGB_READ_LOOP(rescale[*bufferptr++],
                         ptr[aindex] = (1 << cinfo->data_precision) - 1;,
                         bufferptr += source->png_alpha;)
    else
      GRAY_RGB_READ_LOOP(rescale[*bufferptr++], {},
                         bufferptr += source->png_alpha;)
  }
  return 1;
}


METHODDEF(JDIMENSION)
get_gray_cmyk_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit-per-channel or 16-bit-per-channel
 * grayscale or grayscale + alpha PNG files and converting to CMYK.
 */
{
  png_source_ptr source = (png_source_ptr)sinfo;
  register _JSAMPROW ptr;
  register _JSAMPLE *rescale = source->rescale;
  JDIMENSION col;

  get_raw_row(cinfo, sinfo);
  ptr = source->pub._buffer[0];
#if BITS_IN_JSAMPLE != 12
  if (source->png_bit_depth == cinfo->data_precision) {
    register _JSAMPLE *bufferptr = (_JSAMPLE *)source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      _JSAMPLE gray = *bufferptr++;
      bufferptr += source->png_alpha;
      rgb_to_cmyk(_MAXJSAMPLE, gray, gray, gray, ptr, ptr + 1, ptr + 2,
                  ptr + 3);
      ptr += 4;
    }
  } else
#endif
  if (source->png_bit_depth == 16) {
    unsigned short *bufferptr = (unsigned short *)source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      _JSAMPLE gray = rescale[*bufferptr++];
      bufferptr += source->png_alpha;
      rgb_to_cmyk((1 << cinfo->data_precision) - 1, gray, gray, gray, ptr,
                  ptr + 1, ptr + 2, ptr + 3);
      ptr += 4;
    }
  } else {
    register unsigned char *bufferptr = source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      _JSAMPLE gray = rescale[*bufferptr++];
      bufferptr += source->png_alpha;
      rgb_to_cmyk((1 << cinfo->data_precision) - 1, gray, gray, gray, ptr,
                  ptr + 1, ptr + 2, ptr + 3);
      ptr += 4;
    }
  }
  return 1;
}


#define RGB_READ_LOOP(read_op, alpha_set_op, pointer_op) { \
  for (col = cinfo->image_width; col > 0; col--) { \
    ptr[rindex] = read_op; \
    ptr[gindex] = read_op; \
    ptr[bindex] = read_op; \
    alpha_set_op \
    pointer_op \
    ptr += ps; \
  } \
}


METHODDEF(JDIMENSION)
get_rgb_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit-per-channel or 16-bit-per-channel
 * truecolor or truecolor + alpha PNG files and converting to extended RGB.
 */
{
  png_source_ptr source = (png_source_ptr)sinfo;
  register _JSAMPROW ptr;
  register _JSAMPLE *rescale = source->rescale;
  JDIMENSION col;
  register int rindex = rgb_red[cinfo->in_color_space];
  register int gindex = rgb_green[cinfo->in_color_space];
  register int bindex = rgb_blue[cinfo->in_color_space];
  register int aindex = alpha_index[cinfo->in_color_space];
  register int ps = rgb_pixelsize[cinfo->in_color_space];

  get_raw_row(cinfo, sinfo);
  ptr = source->pub._buffer[0];
#if BITS_IN_JSAMPLE != 12
  if (source->png_bit_depth == cinfo->data_precision) {
    register _JSAMPLE *bufferptr = (_JSAMPLE *)source->iobuffer;
    if (aindex >= 0)
      RGB_READ_LOOP(*bufferptr++, ptr[aindex] = _MAXJSAMPLE;,
                    bufferptr += source->png_alpha;)
    else
      RGB_READ_LOOP(*bufferptr++, {}, bufferptr += source->png_alpha;)
  } else
#endif
  if (source->png_bit_depth == 16) {
    unsigned short *bufferptr = (unsigned short *)source->iobuffer;
    if (aindex >= 0)
      RGB_READ_LOOP(rescale[*bufferptr++],
                    ptr[aindex] = (1 << cinfo->data_precision) - 1;,
                    bufferptr += source->png_alpha;)
    else
      RGB_READ_LOOP(rescale[*bufferptr++], {},
                    bufferptr += source->png_alpha;)
  } else {
    register unsigned char *bufferptr = source->iobuffer;
    if (aindex >= 0)
      RGB_READ_LOOP(rescale[*bufferptr++],
                    ptr[aindex] = (1 << cinfo->data_precision) - 1;,
                    bufferptr += source->png_alpha;)
    else
      RGB_READ_LOOP(rescale[*bufferptr++], {},
                    bufferptr += source->png_alpha;)
  }
  return 1;
}


METHODDEF(JDIMENSION)
get_rgb_cmyk_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading 8-bit-per-channel or 16-bit-per-channel
 * truecolor or truecolor + alpha PNG files and converting to CMYK.
 */
{
  png_source_ptr source = (png_source_ptr)sinfo;
  register _JSAMPROW ptr;
  register _JSAMPLE *rescale = source->rescale;
  JDIMENSION col;

  get_raw_row(cinfo, sinfo);
  ptr = source->pub._buffer[0];
#if BITS_IN_JSAMPLE != 12
  if (source->png_bit_depth == cinfo->data_precision) {
    register _JSAMPLE *bufferptr = (_JSAMPLE *)source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      _JSAMPLE r = *bufferptr++;
      _JSAMPLE g = *bufferptr++;
      _JSAMPLE b = *bufferptr++;
      bufferptr += source->png_alpha;
      rgb_to_cmyk(_MAXJSAMPLE, r, g, b, ptr, ptr + 1, ptr + 2, ptr + 3);
      ptr += 4;
    }
  } else
#endif
  if (source->png_bit_depth == 16) {
    unsigned short *bufferptr = (unsigned short *)source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      _JSAMPLE r = rescale[*bufferptr++];
      _JSAMPLE g = rescale[*bufferptr++];
      _JSAMPLE b = rescale[*bufferptr++];
      bufferptr += source->png_alpha;
      rgb_to_cmyk((1 << cinfo->data_precision) - 1, r, g, b, ptr, ptr + 1,
                  ptr + 2, ptr + 3);
      ptr += 4;
    }
  } else {
    register unsigned char *bufferptr = source->iobuffer;
    for (col = cinfo->image_width; col > 0; col--) {
      _JSAMPLE r = rescale[*bufferptr++];
      _JSAMPLE g = rescale[*bufferptr++];
      _JSAMPLE b = rescale[*bufferptr++];
      bufferptr += source->png_alpha;
      rgb_to_cmyk((1 << cinfo->data_precision) - 1, r, g, b, ptr, ptr + 1,
                  ptr + 2, ptr + 3);
      ptr += 4;
    }
  }
  return 1;
}


METHODDEF(JDIMENSION)
get_indexed_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
/* This version is for reading 8-bit-per-channel indexed-color PNG files and
 * converting to extended RGB or CMYK.
 */
  png_source_ptr source = (png_source_ptr)sinfo;
  register _JSAMPROW ptr;
  register JSAMPLE *bufferptr = (JSAMPLE *)source->iobuffer;
  register _JSAMPLE *rescale = source->rescale;
  JDIMENSION col;

  get_raw_row(cinfo, sinfo);
  ptr = source->pub._buffer[0];
#if BITS_IN_JSAMPLE == 8
  if (source->png_bit_depth == cinfo->data_precision) {
    if (cinfo->in_color_space == JCS_GRAYSCALE) {
      for (col = cinfo->image_width; col > 0; col--) {
        JSAMPLE index = *bufferptr++;

        if (index >= source->colormap.n_entries)
          ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
        *ptr++ = source->colormap.entries[index].red;
      }
    } else if (cinfo->in_color_space == JCS_CMYK) {
      for (col = cinfo->image_width; col > 0; col--) {
        JSAMPLE index = *bufferptr++;

        if (index >= source->colormap.n_entries)
          ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
        rgb_to_cmyk(_MAXJSAMPLE, source->colormap.entries[index].red,
                    source->colormap.entries[index].green,
                    source->colormap.entries[index].blue, ptr, ptr + 1,
                    ptr + 2, ptr + 3);
        ptr += 4;
      }
    } else {
      register int rindex = rgb_red[cinfo->in_color_space];
      register int gindex = rgb_green[cinfo->in_color_space];
      register int bindex = rgb_blue[cinfo->in_color_space];
      register int aindex = alpha_index[cinfo->in_color_space];
      register int ps = rgb_pixelsize[cinfo->in_color_space];

      for (col = cinfo->image_width; col > 0; col--) {
        JSAMPLE index = *bufferptr++;

        if (index >= source->colormap.n_entries)
          ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
        ptr[rindex] = source->colormap.entries[index].red;
        ptr[gindex] = source->colormap.entries[index].green;
        ptr[bindex] = source->colormap.entries[index].blue;
        if (aindex >= 0)
          ptr[aindex] = _MAXJSAMPLE;
        ptr += ps;
      }
    }
  } else
#endif
  {
    if (cinfo->in_color_space == JCS_GRAYSCALE) {
      for (col = cinfo->image_width; col > 0; col--) {
        JSAMPLE index = *bufferptr++;

        if (index >= source->colormap.n_entries)
          ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
        *ptr++ = rescale[source->colormap.entries[index].red];
      }
    } else if (cinfo->in_color_space == JCS_CMYK) {
      for (col = cinfo->image_width; col > 0; col--) {
        JSAMPLE index = *bufferptr++;

        if (index >= source->colormap.n_entries)
          ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
        rgb_to_cmyk((1 << cinfo->data_precision) - 1,
                    rescale[source->colormap.entries[index].red],
                    rescale[source->colormap.entries[index].green],
                    rescale[source->colormap.entries[index].blue], ptr,
                    ptr + 1, ptr + 2, ptr + 3);
        ptr += 4;
      }
    } else {
      register int rindex = rgb_red[cinfo->in_color_space];
      register int gindex = rgb_green[cinfo->in_color_space];
      register int bindex = rgb_blue[cinfo->in_color_space];
      register int aindex = alpha_index[cinfo->in_color_space];
      register int ps = rgb_pixelsize[cinfo->in_color_space];

      for (col = cinfo->image_width; col > 0; col--) {
        JSAMPLE index = *bufferptr++;

        if (index >= source->colormap.n_entries)
          ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
        ptr[rindex] = rescale[source->colormap.entries[index].red];
        ptr[gindex] = rescale[source->colormap.entries[index].green];
        ptr[bindex] = rescale[source->colormap.entries[index].blue];
        if (aindex >= 0)
          ptr[aindex] = (1 << cinfo->data_precision) - 1;
        ptr += ps;
      }
    }
  }
  return 1;
}


#ifdef ZERO_BUFFERS

static void *spng_malloc(size_t size)
{
  return calloc(1, size);
}

#endif


/*
 * Read the file header; return image size and component count.
 */

METHODDEF(void)
start_input_png(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  png_source_ptr source = (png_source_ptr)sinfo;
  struct spng_ihdr ihdr;
  int png_components = 3;
  boolean use_raw_buffer;
#ifdef ZERO_BUFFERS
  struct spng_alloc alloc;

  alloc.malloc_fn = spng_malloc;
  alloc.realloc_fn = realloc;
  alloc.calloc_fn = calloc;
  alloc.free_fn = free;
  source->ctx = spng_ctx_new2(&alloc, 0);
#else
  source->ctx = spng_ctx_new(0);
#endif
  if (!source->ctx)
    ERREXITS(cinfo, JERR_PNG_LIBSPNG, "Could not create context");

  TRY_SPNG(spng_set_png_file(source->ctx, sinfo->input_file));

  TRY_SPNG(spng_get_ihdr(source->ctx, &ihdr));

  if (ihdr.width > JPEG_MAX_DIMENSION || ihdr.height > JPEG_MAX_DIMENSION)
    ERREXIT1(cinfo, JERR_IMAGE_TOO_BIG, JPEG_MAX_DIMENSION);
  if (ihdr.bit_depth != 8 && ihdr.bit_depth != 16)
    ERREXIT(cinfo, JERR_PNG_BADDEPTH);
  if (sinfo->max_pixels &&
      (unsigned long long)ihdr.width * ihdr.height > sinfo->max_pixels)
    ERREXIT1(cinfo, JERR_IMAGE_TOO_BIG, sinfo->max_pixels);

  cinfo->image_width = (JDIMENSION)ihdr.width;
  cinfo->image_height = (JDIMENSION)ihdr.height;
  source->png_bit_depth = ihdr.bit_depth;
  source->png_color_type = ihdr.color_type;

  /* initialize flags to most common settings */
  use_raw_buffer = FALSE;       /* do we map input buffer onto I/O buffer? */

  switch (ihdr.color_type) {
  case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
  case SPNG_COLOR_TYPE_GRAYSCALE:
    if (cinfo->in_color_space == JCS_UNKNOWN ||
        cinfo->in_color_space == JCS_RGB)
      cinfo->in_color_space = JCS_GRAYSCALE;
    TRACEMS3(cinfo, 1, JTRC_PNG_GRAYSCALE, ihdr.width, ihdr.height,
             ihdr.bit_depth);
    if (cinfo->in_color_space == JCS_GRAYSCALE) {
      if (ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE &&
          cinfo->data_precision == ihdr.bit_depth) {
        source->pub.get_pixel_rows = get_raw_row;
        use_raw_buffer = TRUE;
      } else
        source->pub.get_pixel_rows = get_gray_row;
    } else if (IsExtRGB(cinfo->in_color_space))
      source->pub.get_pixel_rows = get_gray_rgb_row;
    else if (cinfo->in_color_space == JCS_CMYK)
        source->pub.get_pixel_rows = get_gray_cmyk_row;
    else
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    if (ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA) {
      png_components = 2;
      source->png_alpha = 1;
    } else {
      png_components = 1;
      source->png_alpha = 0;
    }
    break;

  case SPNG_COLOR_TYPE_TRUECOLOR:
  case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
    if (cinfo->in_color_space == JCS_UNKNOWN)
      cinfo->in_color_space = JCS_EXT_RGB;
    TRACEMS3(cinfo, 1, JTRC_PNG_TRUECOLOR, ihdr.width, ihdr.height,
             ihdr.bit_depth);
    if (ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR &&
        cinfo->data_precision == ihdr.bit_depth &&
#if RGB_RED == 0 && RGB_GREEN == 1 && RGB_BLUE == 2 && RGB_PIXELSIZE == 3
        (cinfo->in_color_space == JCS_EXT_RGB ||
         cinfo->in_color_space == JCS_RGB)) {
#else
        cinfo->in_color_space == JCS_EXT_RGB) {
#endif
      source->pub.get_pixel_rows = get_raw_row;
      use_raw_buffer = TRUE;
    } else if (IsExtRGB(cinfo->in_color_space))
      source->pub.get_pixel_rows = get_rgb_row;
    else if (cinfo->in_color_space == JCS_CMYK)
      source->pub.get_pixel_rows = get_rgb_cmyk_row;
    else
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    if (ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA) {
      png_components = 4;
      source->png_alpha = 1;
    } else {
      png_components = 3;
      source->png_alpha = 0;
    }
    break;

  case SPNG_COLOR_TYPE_INDEXED:
  {
    int i, gray = 1;

    TRACEMS3(cinfo, 1, JTRC_PNG_INDEXED, ihdr.width, ihdr.height,
             ihdr.bit_depth);
    TRY_SPNG(spng_get_plte(source->ctx, &source->colormap));
    if (source->png_bit_depth != 8 || source->colormap.n_entries > 256)
      ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);

    for (i = 0; i < (int)source->colormap.n_entries; i++) {
      if (source->colormap.entries[i].red !=
          source->colormap.entries[i].green ||
          source->colormap.entries[i].green !=
          source->colormap.entries[i].blue)
        gray = 0;
    }

    if ((cinfo->in_color_space == JCS_UNKNOWN ||
         cinfo->in_color_space == JCS_RGB) && gray)
      cinfo->in_color_space = JCS_GRAYSCALE;
    if (cinfo->in_color_space == JCS_GRAYSCALE && !gray)
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);

    source->pub.get_pixel_rows = get_indexed_row;
    png_components = 1;
    source->png_alpha = 0;
    break;
  }

  default:
    ERREXIT(cinfo, JERR_PNG_OUTOFRANGE);
  }

  if (IsExtRGB(cinfo->in_color_space))
    cinfo->input_components = rgb_pixelsize[cinfo->in_color_space];
  else if (cinfo->in_color_space == JCS_GRAYSCALE)
    cinfo->input_components = 1;
  else if (cinfo->in_color_space == JCS_CMYK)
    cinfo->input_components = 4;

  /* Allocate space for I/O buffer: 1 or 3 bytes or words/pixel. */
  source->buffer_width =
    (size_t)ihdr.width * png_components * source->png_bit_depth / 8;
  source->iobuffer = (unsigned char *)
    (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                source->buffer_width);

  /* Create compressor input buffer. */
  if (use_raw_buffer) {
    /* For unscaled raw-input case, we can just map it onto the I/O buffer. */
    /* Synthesize a _JSAMPARRAY pointer structure */
    source->pixrow = (_JSAMPROW)source->iobuffer;
    source->pub._buffer = &source->pixrow;
    source->pub.buffer_height = 1;
  } else {
    unsigned int maxval = source->png_bit_depth == 16 ? 65535 : 255;
    size_t val, half_maxval;

    /* Need to translate anyway, so make a separate sample buffer. */
    source->pub._buffer = (_JSAMPARRAY)(*cinfo->mem->alloc_sarray)
      ((j_common_ptr)cinfo, JPOOL_IMAGE,
       (JDIMENSION)ihdr.width * cinfo->input_components, (JDIMENSION)1);
    source->pub.buffer_height = 1;

    /* Compute the rescaling array. */
    source->rescale = (_JSAMPLE *)
      (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                  (maxval + 1L) * sizeof(_JSAMPLE));
    memset(source->rescale, 0, (maxval + 1L) * sizeof(_JSAMPLE));
    half_maxval = maxval / 2;
    for (val = 0; val <= maxval; val++) {
      /* The multiplication here must be done in 32 bits to avoid overflow */
      source->rescale[val] =
        (_JSAMPLE)((val * ((1 << cinfo->data_precision) - 1) + half_maxval) /
                   maxval);
    }
  }

  TRY_SPNG(spng_decode_image(source->ctx, NULL, 0, SPNG_FMT_PNG,
                             SPNG_DECODE_PROGRESSIVE));
}


/*
 * Return the ICC profile (if any) embedded in the PNG image.
 */

METHODDEF(boolean)
read_icc_profile_png(j_compress_ptr cinfo, cjpeg_source_ptr sinfo,
                     JOCTET **icc_data_ptr, unsigned int *icc_data_len)
{
  png_source_ptr source = (png_source_ptr)sinfo;
  struct spng_iccp iccp;

  if (!icc_data_ptr || !icc_data_len)
    ERREXIT(cinfo, JERR_BUFFER_SIZE);

  if (source->ctx && spng_get_iccp(source->ctx, &iccp) == 0) {
    *icc_data_ptr = (JOCTET *)iccp.profile;
    *icc_data_len = (unsigned int)iccp.profile_len;
    return TRUE;
  }

  return FALSE;
}


/*
 * Finish up at the end of the file.
 */

METHODDEF(void)
finish_input_png(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  png_source_ptr source = (png_source_ptr)sinfo;

  if (source->ctx) {
    spng_decode_chunks(source->ctx);
    spng_ctx_free(source->ctx);
    source->ctx = NULL;
  }
}


/*
 * The module selection routine for PNG format input.
 */

GLOBAL(cjpeg_source_ptr)
_jinit_read_png(j_compress_ptr cinfo)
{
  png_source_ptr source;

#if BITS_IN_JSAMPLE == 8
  if (cinfo->data_precision > BITS_IN_JSAMPLE || cinfo->data_precision < 2)
#else
  if (cinfo->data_precision > BITS_IN_JSAMPLE ||
      cinfo->data_precision < BITS_IN_JSAMPLE - 3)
#endif
    ERREXIT1(cinfo, JERR_BAD_PRECISION, cinfo->data_precision);

  /* Create module interface object */
  source = (png_source_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                sizeof(png_source_struct));
  /* Fill in method ptrs, except get_pixel_rows which start_input sets */
  source->pub.start_input = start_input_png;
  source->pub.read_icc_profile = read_icc_profile_png;
  source->pub.finish_input = finish_input_png;
  source->pub.max_pixels = 0;

  return (cjpeg_source_ptr)source;
}

#endif /* defined(PNG_SUPPORTED) &&
          (BITS_IN_JSAMPLE != 16 || defined(C_LOSSLESS_SUPPORTED)) */
