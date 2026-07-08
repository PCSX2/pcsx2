/* mips/check.h - MIPS optimised filter functions
 *
 * Copyright (c) 2024 John Bowler
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

/* Unlike other architectures the tests here are written so that they can be
 * extended by the addition of other different ISA extensions.  This could
 * easily be done with all the other architectures too.
 *
 * TODO: move the ISA specific checks to sub-directories so that the code
 * does not taint other implementations.
 */
#include "msacheck.h" /* Check for MSA extensions */
/*... add other checks here (each in its own header file). */

#ifdef PNG_TARGET_MIPS_TARGET_CODE_SUPPORTED
   /* Regardless of the optimization the following must always be the same: */
#  define PNG_TARGET_CODE_IMPLEMENTATION "mips/mips_init.c"
#  define PNG_TARGET_IMPLEMENTS_FILTERS
#  define PNG_TARGET_ROW_ALIGNMENT 16
#endif
