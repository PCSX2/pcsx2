/* powerpc/check.h - POWERPC optimised filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2017 Glenn Randers-Pehrson
 * Written by Vadim Barkov, 2017.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */
#if defined(__PPC64__) && defined(__ALTIVEC__) && defined(PNG_READ_SUPPORTED)

#include <altivec.h>

#ifdef __VSX__
#  define PNG_TARGET_CODE_IMPLEMENTATION "powerpc/powerpc_init.c"
   /* PNG_TARGET_STORES_DATA */
#  define PNG_TARGET_IMPLEMENTS_FILTERS
   /* PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE */
   /* PNG_TARGET_ROW_ALIGNMENT */
#endif /* __VSX__ */
#endif /* __PPC64__ && __ALTIVEC__ && READ */
