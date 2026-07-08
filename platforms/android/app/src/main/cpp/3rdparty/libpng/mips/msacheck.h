/* mips/msacheck.h - MIPS optimised filter functions
 *
 * Copyright (c) 2018-2022 Cosmin Truta
 * Copyright (c) 2014,2016 Glenn Randers-Pehrson
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * This code has been moved from the original in pngpriv.h.
 */
/* MIPS MSA checks: */
#if defined(__mips_msa) && (__mips_isa_rev >= 5)
   /* MIPS MSA support requires gcc >= 4.7: */
#  ifdef __GNUC__
#     if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
#        define PNG_TARGET_MIPS_MSA_SUPPORTED
#     endif
#  else /* !GNUC */
#     define PNG_TARGET_MIPS_MSA_SUPPORTED
#  endif /* !GNUC */
#endif /* !__mips_msa || __mips_isa_rev < 5 */

#ifdef PNG_TARGET_MIPS_MSA_SUPPORTED
#  define PNG_TARGET_MIPS_TARGET_CODE_SUPPORTED
#endif
