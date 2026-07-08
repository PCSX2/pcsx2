/* mips_init.c - MSA optimised filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2016 Glenn Randers-Pehrson
 * Written by Mandar Sahastrabuddhe, 2016
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
#define png_target_impl "mips-msa"

#include "filter_msa_intrinsics.c"

static void
png_init_filter_functions_msa(png_struct *pp, unsigned int bpp)
{
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_msa;

   if (bpp == 3)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_msa;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_msa;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth3_msa;
   }

   else if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_msa;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_msa;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth4_msa;
   }
}

#  define png_target_init_filter_functions_impl png_init_filter_functions_msa
