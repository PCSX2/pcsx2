/* arm/check.h - NEON optimised filter functions
 *
 * Copyright (c) 2018-2022 Cosmin Truta
 * Copyright (c) 2014,2016 Glenn Randers-Pehrson
 * Written by Mans Rullgard, 2011.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#  define PNG_TARGET_CODE_IMPLEMENTATION "arm/arm_init.c"
#  define PNG_TARGET_IMPLEMENTS_FILTERS
#  ifdef PNG_READ_EXPAND_SUPPORTED
#     define PNG_TARGET_STORES_DATA
#     define PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
#  endif /* READ_EXPAND */
#  define PNG_TARGET_ROW_ALIGNMENT 16
#endif /* ARM_NEON */
