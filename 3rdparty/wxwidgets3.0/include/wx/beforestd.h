///////////////////////////////////////////////////////////////////////////////
// Name:        wx/beforestd.h
// Purpose:     #include before STL headers
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07/07/03
// Copyright:   (c) 2003 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/**
    Unfortunately, when compiling at maximum warning level, the standard
    headers themselves may generate warnings -- and really lots of them. So
    before including them, this header should be included to temporarily
    suppress the warnings and after this the header afterstd.h should be
    included to enable them back again.

    Note that there are intentionally no inclusion guards in this file, because
    it can be included several times.
 */

// VC 7.x isn't as bad as VC6 and doesn't give these warnings but eVC (which
// defines _MSC_VER as 1201) does need to be included as it's VC6-like
#if defined(__VISUALC__) && __VISUALC__ <= 1201
    // these warning have to be disabled and not just temporarily disabled
    // because they will be given at the end of the compilation of the
    // current source and there is absolutely nothing we can do about them so
    // disable them before warning(push) below

    // 'foo': unreferenced inline function has been removed
    #pragma warning(disable:4514)

    // 'function' : function not inlined
    #pragma warning(disable:4710)

    // 'id': identifier was truncated to 'num' characters in the debug info
    #pragma warning(disable:4786)

    // MSVC 5 does not have this
    #if __VISUALC__ > 1100
        // we have to disable (and reenable in afterstd.h) this one because,
        // even though it is of level 4, it is not disabled by warning(push, 1)
        // below for VC7.1!

        // unreachable code
        #pragma warning(disable:4702)

        #pragma warning(push, 1)
    #else // VC 5
        // 'expression' : signed/unsigned mismatch
        #pragma warning(disable:4018)

        // 'identifier' : unreferenced formal parameter
        #pragma warning(disable:4100)

        // 'conversion' : conversion from 'type1' to 'type2',
        // possible loss of data
        #pragma warning(disable:4244)

        // C++ language change: to explicitly specialize class template
        // 'identifier' use the following syntax
        #pragma warning(disable:4663)
    #endif
#endif // VC++ < 7

/**
    GCC's visibility support is broken for libstdc++ in some older versions
    (namely Debian/Ubuntu's GCC 4.1, see
    https://bugs.launchpad.net/ubuntu/+source/gcc-4.1/+bug/109262). We fix it
    here by mimicking newer versions' behaviour of using default visibility
    for libstdc++ code.
 */
#if defined(HAVE_VISIBILITY) && defined(HAVE_BROKEN_LIBSTDCXX_VISIBILITY)
    #pragma GCC visibility push(default)
#endif
