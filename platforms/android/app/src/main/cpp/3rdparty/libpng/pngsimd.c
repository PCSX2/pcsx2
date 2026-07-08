/* pngsimd.c - hardware (cpu/arch) specific code
 *
 * Copyright (c) 2018-2024 Cosmin Truta
 * Copyright (c) 1998-2002,2004,2006-2018 Glenn Randers-Pehrson
 * Copyright (c) 1996-1997 Andreas Dilger
 * Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
#include "pngpriv.h"

#ifdef PNG_TARGET_CODE_IMPLEMENTATION
/* This is set by pngtarget.h iff there is some target code to be compiled.
 */

/* Each piece of separate hardware support code must have a "init" file defined
 * in PNG_TARGET_CODE_IMPLEMENTATION and included here.
 *
 * The "check" header set PNG_TARGET_CODE_IMPLEMENTATION and that file *MUST*
 * supply macro definitions as follows.  Note that all functions must be static
 * to avoid clashes with other implementations.
 *
 *    png_target_impl
 *       string constant
 *       REQUIRED
 *       This must be a string naming the implementation.
 *
 *    png_target_free_data_impl
 *       static void png_target_free_data_impl(png_struct *)
 *       REQUIRED if PNG_TARGET_STORES_DATA is defined
 *       UNDEFINED if PNG_TARGET_STORES_DATA is not defined
 *       A function to free data stored in png_struct::target_data.
 *
 *    png_target_init_filter_functions_impl [flag: png_target_filters]
 *       OPTIONAL
 *       Contains code to overwrite the png_struct::read_filter array, see
 *       the definition of png_init_filter_functions.  Need not be defined,
 *       only called if target_state contains png_target_filters.
 *
 *    png_target_do_expand_palette_impl   [flag: png_target_expand_palette]
 *       static function
 *       OPTIONAL
 *       Handles the transform.  Need not be defined, only called if the
 *       state contains png_target_<transform>, may set this flag to zero, may
 *       return false to indicate that the transform was not done (so the
 *       C implementation must then execute).
 *
 * Note that pngtarget.h verifies that at least one thing is implemented, the
 * checks below ensure that the corresponding _impl macro is defined.
 */

/* This will fail in an obvious way with a meaningful error message if the file
 * does not exist:
 */
#include PNG_TARGET_CODE_IMPLEMENTATION

#ifndef png_target_impl
#  error TARGET SPECIFIC CODE: PNG_TARGET_CODE_IMPLEMENTATION is defined but\
 png_hareware_impl is not
#endif

#if defined(PNG_TARGET_STORES_DATA) != defined(png_target_free_data_impl)
#  error TARGET SPECIFIC CODE: png_target_free_data_impl unexpected setting
#endif

#if defined(PNG_TARGET_IMPLEMENTS_FILTERS) !=\
    defined(png_target_init_filter_functions_impl)
#  error TARGET SPECIFIC CODE: png_target_init_filter_functions_impl unexpected\
      setting
#endif

#if defined(PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE) !=\
    defined(png_target_do_expand_palette_impl)
#  error TARGET SPECIFIC CODE: png_target_do_expand_palette_impl unexpected\
      setting
#endif

void
png_target_init(png_struct *pp)
{
   /* Initialize png_struct::target_state if required. */
#  ifdef png_target_init_filter_functions_impl
#     define PNG_TARGET_FILTER_SUPPORT png_target_filters
#  else
#     define PNG_TARGET_FILTER_SUPPORT 0U
#  endif
#  ifdef png_target_do_expand_palette_impl
#     define PNG_TARGET_EXPAND_PALETTE_SUPPORT png_target_expand_palette
#  else
#     define PNG_TARGET_EXPAND_PALETTE_SUPPORT 0U
#  endif

#  define PNG_TARGET_SUPPORT (PNG_TARGET_FILTER_SUPPORT |\
                              PNG_TARGET_EXPAND_PALETTE_SUPPORT)

#  if PNG_TARGET_SUPPORT != 0U
      pp->target_state = PNG_TARGET_SUPPORT;
#  else
      PNG_UNUSED(pp);
#  endif
}

#ifdef PNG_TARGET_STORES_DATA
void
png_target_free_data(png_struct *pp)
{
   /* Free any data allocated in the png_struct::target_data.
    */
   if (pp->target_data != NULL)
   {
      png_target_free_data_impl(pp);
      if (pp->target_data != NULL)
         png_error(pp, png_target_impl ": allocated data not released");
   }
}
#endif

#ifdef PNG_TARGET_IMPLEMENTS_FILTERS
void
png_target_init_filter_functions(png_struct *pp, unsigned int bpp)
{
   if (((pp->options >> PNG_TARGET_SPECIFIC_CODE) & 3) == PNG_OPTION_ON &&
       (pp->target_state & png_target_filters) != 0)
      png_target_init_filter_functions_impl(pp, bpp);
}
#endif /* filters */

#ifdef PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
int
png_target_do_expand_palette(png_struct *pp, png_row_info *rip)
   /*const png_byte *row, const png_byte **ssp, const png_byte **ddp) */
{
   /* This is exactly like 'png_do_expand_palette' except that there is a check
    * on the options and target_state:
    */
   return ((pp->options >> PNG_TARGET_SPECIFIC_CODE) & 3) == PNG_OPTION_ON &&
      (pp->target_state & png_target_expand_palette) != 0 &&
      png_target_do_expand_palette_impl(pp, rip, pp->row_buf + 1,
            pp->palette, pp->trans_alpha, pp->num_trans);
}
#endif /* EXPAND_PALETTE */
#endif /* PNG_TARGET_ARCH */
