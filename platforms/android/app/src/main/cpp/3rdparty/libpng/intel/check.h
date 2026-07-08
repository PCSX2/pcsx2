/* intel/check.h - SSE2 optimized filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2016-2017 Glenn Randers-Pehrson
 * Written by Mike Klein and Matt Sarett, Google, Inc.
 * Derived from arm/arm_init.c
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
/* PNG_INTEL_SSE_IMPLEMENTATION is used in the actual implementation to select
 * the correct code.
 */
#if defined(__SSE4_1__) || defined(__AVX__)
   /* We are not actually using AVX, but checking for AVX is the best way we can
    * detect SSE4.1 and SSSE3 on MSVC.
    */
#  define PNG_INTEL_SSE_IMPLEMENTATION 3
#elif defined(__SSSE3__)
#  define PNG_INTEL_SSE_IMPLEMENTATION 2
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) ||\
      (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  define PNG_INTEL_SSE_IMPLEMENTATION 1
#else
#  define PNG_INTEL_SSE_IMPLEMENTATION 0
#endif

#if PNG_INTEL_SSE_IMPLEMENTATION > 0
#  define PNG_TARGET_CODE_IMPLEMENTATION "intel/intel_init.c"
   /*PNG_TARGET_STORES_DATA*/
#  define PNG_TARGET_IMPLEMENTS_FILTERS
   /*PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE*/
#  define PNG_TARGET_ROW_ALIGNMENT 16
#endif /* PNG_INTEL_SSE_IMPLEMENTATION > 0 */
