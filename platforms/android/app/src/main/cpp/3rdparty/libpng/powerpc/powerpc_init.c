/* powerpc_init.c - POWERPC optimised filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2017 Glenn Randers-Pehrson
 * Written by Vadim Barkov, 2017.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
#define png_target_impl "powerpc-vsx"

#include <altivec.h>
#include "filter_vsx_intrinsics.c"

static void
png_init_filter_functions_vsx(png_struct *pp, unsigned int bpp)
{
   /* IMPORTANT: DO NOT DEFINE EXTERNAL FUNCTIONS HERE
    *
    * This is because external functions must be declared with
    * PNG_INTERNAL_FUNCTION in pngpriv.h; without this the PNG_PREFIX option to
    * the build will not work (it will not know about these symbols).
    */
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_vsx;

   if (bpp == 3)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_vsx;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_vsx;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth3_vsx;
   }

   else if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_vsx;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_vsx;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth4_vsx;
   }
}

#define png_target_init_filter_functions_impl png_init_filter_functions_vsx
