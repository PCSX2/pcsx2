///////////////////////////////////////////////////////////////////////////////
// Name:        wx/afterstd.h
// Purpose:     #include after STL headers
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07/07/03
// Copyright:   (c) 2003 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/**
    See the comments in beforestd.h.
 */

#if defined(__WINDOWS__)
    #include "wx/msw/winundef.h"
#endif

// undo what we did in wx/beforestd.h
#if defined(__VISUALC__) && __VISUALC__ <= 1201
    #pragma warning(pop)
#endif

// see beforestd.h for explanation
#if defined(HAVE_VISIBILITY) && defined(HAVE_BROKEN_LIBSTDCXX_VISIBILITY)
    #pragma GCC visibility pop
#endif
