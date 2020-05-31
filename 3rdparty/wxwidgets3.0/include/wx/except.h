///////////////////////////////////////////////////////////////////////////////
// Name:        wx/except.h
// Purpose:     C++ exception related stuff
// Author:      Vadim Zeitlin
// Modified by:
// Created:     17.09.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EXCEPT_H_
#define _WX_EXCEPT_H_

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// macros working whether wxUSE_EXCEPTIONS is 0 or 1
// ----------------------------------------------------------------------------

// even if the library itself was compiled with exceptions support, the user
// code using it might be compiling with a compiler switch disabling them in
// which cases we shouldn't use try/catch in the headers -- this results in
// compilation errors in e.g. wx/scopeguard.h with at least g++ 4
#if !wxUSE_EXCEPTIONS || \
        (defined(__GNUG__) && !defined(__EXCEPTIONS))
    #ifndef wxNO_EXCEPTIONS
        #define wxNO_EXCEPTIONS
    #endif
#endif

#ifdef wxNO_EXCEPTIONS
    #define wxTRY
    #define wxCATCH_ALL(code)
#else // do use exceptions
    #define wxTRY try
    #define wxCATCH_ALL(code) catch ( ... ) { code }
#endif // wxNO_EXCEPTIONS/!wxNO_EXCEPTIONS

#endif // _WX_EXCEPT_H_

