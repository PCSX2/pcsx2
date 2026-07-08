/* pngtarget.h - target configuration file for libpng
 *
 * libpng version 1.6.44.git
 *
 * Copyright (c) 2024 John Bowler
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * [[Added to libpng1.8]]
 *
 * This header file discovers whether the target machine has support for target
 * (normally CPU) specific code such as SIMD instructions.  It is included by
 * pngpriv.h immediately after pnglibconf.h to establish compile-time (as
 * opposed to configuration time) requirements for the build of libpng
 *
 * The header only defines a very limited number of macros and it only defines
 * macros; no functions are declared, no types etc.
 *
 * Every target architecture <arch> must have the following file:
 *
 *    <arch>/check.h
 *
 * This file contains checks based on compiler flags to determine if
 * target-specific code can be implemented for this architecture with this set
 * of compiler options.  Define
 *
 *    PNG_TARGET_CODE_IMPLEMENTATION
 *
 * To the quoted relative path name of a single C file to include to obtain the
 * implementation of the target specific code.  For example:
 *
 *    "arm/arm_init.c"
 *    "intel/intel_init.c"
 *
 * This file will be included by pngsmid.c so the string must be a valid
 * relative path name from that file.  See the file pngsmid.c for the definition
 * of what the C file must do.
 *
 * When it defines PNG_TARGET_CODE_IMPLEMENTATION the check file may also
 * define:
 *
 *    PNG_TARGET_STORES_DATA
 *       If set a void *pointer called "target_data" will be defined in
 *       pngstruct.h.  The initialization code included in pngsimd.c must then
 *       also implement a function to free the data called png_target_free_data,
 *       see png_simd.c.
 *
 *    PNG_TARGET_ROW_ALIGNMENT
 *       If set this defines a power-of-2 required memory alignment for rows
 *       passed to the read "filter".  If not set this defaults to 1.
 *
 *    PNG_TARGET_IMPLEMENTS_FILTERS
 *       If defined this indicates to the system that target specific
 *       implementations of the read filters may be available.  This must be set
 *       to cause a target specific filter implementation to be used.
 *
 *    PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
 *       If defined this indicates to the system that target specific
 *       code for rgb_do_expand_palette is available.  This must be defined to
 *       cause such implementations to be used.
 *
 * It MUST NOT define these macros unless it also defines
 * PNG_TARGET_CODE_IMPLEMENTATION.  At least one of the 'IMPLEMENTS' macros must
 * be defined; this file will produce an error diagnostic if not.
 *
 * If the check.h file needs to define other macros, for example for use in the
 * PNG_TARGET_CODE_IMPLEMENTATION file macros must have the form:
 *
 *    PNG_TARGET_<ARCH>_...
 *
 * Where ARCH the architecture directory (the directory containing check.h) in
 * upper case.  See pngsimd.c for more information about function definitions
 * used to implement the code.
 */
#ifndef PNGTARGET_H
#define PNGTARGET_H

#ifdef PNG_TARGET_SPECIFIC_CODE_SUPPORTED /* from pnglibconf.h */
#  ifdef PNG_READ_SUPPORTED /* checked here as a convenience */
#     include "arm/check.h"
#     include "intel/check.h"
#     include "mips/check.h"
#     include "powerpc/check.h"
#endif
#endif /* PNG_TARGET_SPECIFIC_CODE_SUPPORTED */

/* This is also a convenience to avoid checking in every check.h: */
#ifndef PNG_READ_EXPAND_SUPPORTED
#  undef PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
#endif

/* Now check the condition above.  Note that these checks consider the composite
 * result of all the above includes; if errors are preceded by warnings about
 * redefinition of the macros those need to be fixed first.
 */
#ifdef PNG_TARGET_CODE_IMPLEMENTATION /* There is target-specific code */
/* List all the supported target specific code types here: */
#  if !defined(PNG_TARGET_IMPLEMENTS_FILTERS) &&\
      !defined(PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE)
#  error PNG_TARGET_CODE_IMPLEMENTATION without any implementations.

/* Currently only row alignments which are a power of 2 and less than 17 are
 * supported: the current code always aligns to 16 bytes (but may not in the
 * future).
 */
#  if defined(PNG_TARGET_ROW_ALIGNMENT) && (\
      PNG_TARGET_ROW_ALIGNMENT > 16 /*too big*/ ||\
      PNG_TARGET_ROW_ALIGNMENT !=\
      (PNG_TARGET_ROW_ALIGNMENT & -PNG_TARGET_ROW_ALIGNMENT)) /*!power of 2*/
#     error unsupported TARGET_ROW_ALIGNMENT
#  endif /* PNG_TARGET_ROW_ALIGNMENT check */
#endif /* Target specific code macro checks. */
#endif /* PNG_TARGET_SPECIFIC_CODE_SUPPORTED */

#ifndef PNG_TARGET_CODE_IMPLEMENTATION
#  if defined(PNG_TARGET_STORES_DATA) ||\
      defined(PNG_TARGET_ROW_ALIGNMENT) ||\
      defined(PNG_TARGET_IMPLEMENTS_FILTERS) ||\
      defined(PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE)
#     error PNG_TARGET_ macro defined without target specfic code.
#  endif /* Check PNG_TARGET_ macros are not defined. */
#endif /* PNG_TARGET_CODE_IMPLEMENTATION */

#ifndef PNG_TARGET_ROW_ALIGNMENT
#  define PNG_TARGET_ROW_ALIGNMENT 1
#endif
#endif /* PNGTARGET_H */
