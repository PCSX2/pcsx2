/*
 * Name:        wx/compiler.h
 * Purpose:     Compiler-specific macro definitions.
 * Author:      Vadim Zeitlin
 * Created:     2013-07-13 (extracted from wx/platform.h)
 * Copyright:   (c) 1997-2013 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_COMPILER_H_
#define _WX_COMPILER_H_

/*
    Compiler detection and related helpers.
 */

/*
    Notice that Intel compiler can be used as Microsoft Visual C++ add-on and
    so we should define both __INTELC__ and __VISUALC__ for it.
*/
#ifdef __INTEL_COMPILER
#   define __INTELC__
#endif

#if defined(_MSC_VER)
    /*
       define another standard symbol for Microsoft Visual C++: the standard
       one (_MSC_VER) is also defined by some other compilers.
     */
#   define __VISUALC__ _MSC_VER

    /*
      define special symbols for different VC version instead of writing tests
      for magic numbers such as 1200, 1300 &c repeatedly
    */
#if __VISUALC__ < 1100
#   error "This Visual C++ version is too old and not supported any longer."
#elif __VISUALC__ < 1200
#   define __VISUALC5__
#elif __VISUALC__ < 1300
#   define __VISUALC6__
#elif __VISUALC__ < 1400
#   define __VISUALC7__
#elif __VISUALC__ < 1500
#   define __VISUALC8__
#elif __VISUALC__ < 1600
#   define __VISUALC9__
#elif __VISUALC__ < 1700
#   define __VISUALC10__
#elif __VISUALC__ < 1800
#   define __VISUALC11__
#elif __VISUALC__ < 1900
#   define __VISUALC12__
#else
#   pragma message("Please update wx/compiler.h to recognize this VC++ version")
#endif

#elif defined(__BCPLUSPLUS__) && !defined(__BORLANDC__)
#   define __BORLANDC__
#elif defined(__WATCOMC__)
#elif defined(__SC__)
#   define __SYMANTECC__
#elif defined(__SUNPRO_CC)
#   ifndef __SUNCC__
#       define __SUNCC__ __SUNPRO_CC
#   endif /* Sun CC */
#elif defined(__SC__)
#    ifdef __DMC__
#         define __DIGITALMARS__
#    else
#         define __SYMANTEC__
#    endif
#endif  /* compiler */

/*
   Macros for checking compiler version.
*/

/*
   This macro can be used to test the gcc version and can be used like this:

#    if wxCHECK_GCC_VERSION(3, 1)
        ... we have gcc 3.1 or later ...
#    else
        ... no gcc at all or gcc < 3.1 ...
#    endif
*/
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
    #define wxCHECK_GCC_VERSION( major, minor ) \
        ( ( __GNUC__ > (major) ) \
            || ( __GNUC__ == (major) && __GNUC_MINOR__ >= (minor) ) )
#else
    #define wxCHECK_GCC_VERSION( major, minor ) 0
#endif

/*
   This macro can be used to test the Visual C++ version.
*/
#ifndef __VISUALC__
#   define wxVISUALC_VERSION(major) 0
#   define wxCHECK_VISUALC_VERSION(major) 0
#else
#   define wxVISUALC_VERSION(major) ( (6 + major) * 100 )
#   define wxCHECK_VISUALC_VERSION(major) ( __VISUALC__ >= wxVISUALC_VERSION(major) )
#endif

/**
    This is similar to wxCHECK_GCC_VERSION but for Sun CC compiler.
 */
#ifdef __SUNCC__
    /*
       __SUNCC__ is 0xVRP where V is major version, R release and P patch level
     */
    #define wxCHECK_SUNCC_VERSION(maj, min) (__SUNCC__ >= (((maj)<<8) | ((min)<<4)))
#else
    #define wxCHECK_SUNCC_VERSION(maj, min) (0)
#endif

#ifndef __WATCOMC__
#   define wxWATCOM_VERSION(major,minor) 0
#   define wxCHECK_WATCOM_VERSION(major,minor) 0
#   define wxONLY_WATCOM_EARLIER_THAN(major,minor) 0
#   define WX_WATCOM_ONLY_CODE( x )
#else
#   if __WATCOMC__ < 1200
#       error "Only Open Watcom is supported in this release"
#   endif

#   define wxWATCOM_VERSION(major,minor) ( major * 100 + minor * 10 + 1100 )
#   define wxCHECK_WATCOM_VERSION(major,minor) ( __WATCOMC__ >= wxWATCOM_VERSION(major,minor) )
#   define wxONLY_WATCOM_EARLIER_THAN(major,minor) ( __WATCOMC__ < wxWATCOM_VERSION(major,minor) )
#   define WX_WATCOM_ONLY_CODE( x )  x
#endif

/*
   This macro can be used to check that the version of mingw32 CRT is at least
   maj.min
 */

/*
    Define Mingw identification symbols, wxCHECK_MINGW32_VERSION() is defined in
    wx/msw/gccpriv.h which is included later, see comments there.
 */
#ifdef __MINGW32__
/*
    MinGW-w64 project provides compilers for both Win32 and Win64 but only
    defines the same __MINGW32__ symbol for the former as MinGW32 toolchain
    which is quite different (notably doesn't provide many SDK headers that
    MinGW-w64 does include). So we define a separate symbol which, unlike the
    predefined __MINGW64__, can be used to detect this toolchain in both 32 and
    64 bit builds.

    And define __MINGW32_TOOLCHAIN__ for consistency and also because it's
    convenient as we often want to have some workarounds only for the (old)
    MinGW32 but not (newer) MinGW-w64, which still predefines __MINGW32__.
 */
#   ifdef __MINGW64_VERSION_MAJOR
#       ifndef __MINGW64_TOOLCHAIN__
#           define __MINGW64_TOOLCHAIN__
#       endif
#   else
#       ifndef __MINGW32_TOOLCHAIN__
#           define __MINGW32_TOOLCHAIN__
#       endif
#   endif
#endif

#endif // _WX_COMPILER_H_
