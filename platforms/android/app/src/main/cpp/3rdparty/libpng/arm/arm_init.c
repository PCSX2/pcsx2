/* arm_init.c - NEON optimised filter functions
 *
 * Copyright (c) 2018-2022 Cosmin Truta
 * Copyright (c) 2014,2016 Glenn Randers-Pehrson
 * Written by Mans Rullgard, 2011.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
#define png_target_impl "arm-neon"

#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#  include <arm64_neon.h>
#else
#  include <arm_neon.h>
#endif

/* Obtain the definitions of the actual filter functions: */
#include "filter_neon_intrinsics.c"

static void
png_init_filter_functions_neon(png_struct *pp, unsigned int bpp)
{
   png_debug(1, "in png_init_filter_functions_neon");

   /* IMPORTANT: DO NOT DEFINE EXTERNAL FUNCTIONS HERE
    *
    * This is because external functions must be declared with
    * PNG_INTERNAL_FUNCTION in pngpriv.h; without this the PNG_PREFIX option to
    * the build will not work (it will not know about these symbols).
    */
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_neon;

   if (bpp == 3)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_neon;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_neon;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] =
         png_read_filter_row_paeth3_neon;
   }

   else if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_neon;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_neon;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] =
          png_read_filter_row_paeth4_neon;
   }
}

#define png_target_init_filter_functions_impl png_init_filter_functions_neon

#ifdef PNG_TARGET_STORES_DATA
/*    png_target_free_data_impl
 *       Must be defined if the implementation stores data in
 *       png_struct::target_data.  Need not be defined otherwise.
 */
static void
png_target_free_data_arm(png_struct *pp)
{
   void *ptr = pp->target_data;
   pp->target_data = NULL;
   png_free(pp, ptr);
}
#define png_target_free_data_impl png_target_free_data_arm
#endif /* TARGET_STORES_DATA */

#ifdef PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
/*    png_target_do_expand_palette_impl   [flag: png_target_expand_palette]
 *       static function
 *       OPTIONAL
 *       Handles the transform.  Need not be defined, only called if the
 *       state contains png_target_<transform>, may set this flag to zero, may
 *       return false to indicate that the transform was not done (so the
 *       C implementation must then execute).
 */
#include "palette_neon_intrinsics.c"

static int
png_target_do_expand_palette_neon(png_struct *png_ptr, png_row_info *row_info,
    png_byte *row, const png_color *palette, const png_byte *trans_alpha,
    int num_trans)
{
   /* NOTE: it is important that this is done. row_info->width is not a CSE
    * because the pointer is not declared with the 'restrict' parameter, this
    * makes it a CSE but then it is very important that no one changes it in
    * this function, hence the const.
    */
   const png_uint_32 row_width = row_info->width;

   /* NOTE: this is pretty much the original code:
    *
    * 1) The original code only works when the original PNG has 8-bits per
    *    palette.  This test was in pngrtran.c and is now here.
    *
    * 2) The original code starts at the end and works backward but then stops
    *    when it is within 16 bytes of the start.  It then left the remainder to
    *    the original code in pngrtran.c  That code is now here.
    *
    * 3) The original code takes pointers to the end of the input and the end of
    *    the output; this is the way png_do_expand_palette works because it
    *    has to copy down from the end (otherwise it would overwrite the input
    *    data before it read it).  Note that the row buffer is aliased by
    *    these two pointers.
    *
    *    A consequence of passing pointers is that the row pointers (input and
    *    output) are forced into memory (they can't be in registers).  This
    *    could be fixed and some compilers may be able to handle this but
    *    no changes have been made to the original ARM code at this point.
    */
   if (row_info->color_type == PNG_COLOR_TYPE_PALETTE &&
       row_info->bit_depth == 8 /* <8 requires a bigger "riffled" palette */)
   {
      const png_byte *sp = row + (row_width - 1); /* 8 bit palette index */
      if (num_trans > 0)
      {
         /* This case needs a "riffled" palette.  In this implementation the
          * initialization is done here, on demand.
          */
         if (png_ptr->target_data == NULL)
         {
            /* Initialize the accelerated palette expansion.
             *
             * The data is now allocated using png_malloc_warn so the code
             * does not error out on OOM.
             */
            png_ptr->target_data = png_malloc_warn(png_ptr, 256 * 4);

            /* On allocation error it is essential to clear the flag or a
             * massive number of warnings will be output.
             */
            if (png_ptr->target_data != NULL)
               png_riffle_palette_neon(png_ptr->target_data, palette,
                     trans_alpha, num_trans);
            else
               goto clear_flag;
         }

         /* This is the general convention in the core transform code; when
          * expanding the number of bytes in the row copy down (necessary) and
          * pass a pointer to the last byte, not the first.
          *
          * It does not have to be preserved here but maybe it is better this
          * way despite the fact that the comments in the neon palette code
          * obfuscate what is happening.
          */
         png_byte *dp = row + (4/*RGBA*/*row_width - 1);

         /* Cosmin Truta: "Sometimes row_info->bit_depth has been changed to 8.
          * In these cases, the palette hasn't been riffled."
          *
          * John Bowler: Explanation: The code in png_do_palette_expand
          * *invariably* changes the bit depth to 8.  So low palette bit depth
          * gets expanded to 8 and png_row_info is adjusted to reflect this (see
          * png_do_palette_expand), however the "riffle" initialization code
          * checked the original png_ptr bit depth, so it didn't know this would
          * happen...
          *
          * This could be changed; the original bit depth is irrelevant to the
          * initialization code.
          */
          png_uint_32 i = png_target_do_expand_palette_rgba8_neon(
                png_ptr->target_data, row_info->width, &sp, &dp);

          if (i == 0) /* nothing was done */
             return 0; /* Return here: interlaced images start out narrow */

         /* Now 'i' make not have reached row_width.
          * NOTE: [i] is not the index into the row buffer, rather than is
          * [row_width-i], this is the way it is done in the original
          * png_do_expand_palette.
          */
         for (; i < row_width; i++)
         {
            if ((int)(*sp) >= num_trans)
               *dp-- = 0xff;
            else
               *dp-- = trans_alpha[*sp];
            *dp-- = palette[*sp].blue;
            *dp-- = palette[*sp].green;
            *dp-- = palette[*sp].red;
            sp--;
         }

         /* Finally update row_info to reflect the expanded output: */
         row_info->bit_depth = 8;
         row_info->pixel_depth = 32;
         row_info->rowbytes = row_width * 4;
         row_info->color_type = 6;
         row_info->channels = 4;
         return 1;
      }
      else
      {
         /* No tRNS chunk (num_trans == 0), expand to RGB not RGBA. */
         png_byte *dp = row + (3/*RGB*/*row_width - 1);

         png_uint_32 i = png_target_do_expand_palette_rgb8_neon(palette,
               row_info->width, &sp, &dp);

         if (i == 0)
            return 0; /* Return here: interlaced images start out narrow */

         /* Finish the last bytes: */
         for (; i < row_width; i++)
         {
            *dp-- = palette[*sp].blue;
            *dp-- = palette[*sp].green;
            *dp-- = palette[*sp].red;
            sp--;
         }

         row_info->bit_depth = 8;
         row_info->pixel_depth = 24;
         row_info->rowbytes = row_width * 3;
         row_info->color_type = 2;
         row_info->channels = 3;
         return 1;
      }
   }

clear_flag:
   /* Here on malloc failure and on an inapplicable image. */
   png_ptr->target_state &= ~png_target_expand_palette;
   return 0;
}

#define png_target_do_expand_palette_impl png_target_do_expand_palette_neon
/* EXPAND_PALETTE */

#endif /*TODO*/
