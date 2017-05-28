///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wrapgdip.h
// Purpose:     wrapper around <gdiplus.h> header
// Author:      Vadim Zeitlin
// Created:     2007-03-15
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_WRAPGDIP_H_
#define _WX_MSW_WRAPGDIP_H_

#include "wx/msw/wrapwin.h"

// these macros must be defined before gdiplus.h is included but we explicitly
// prevent windows.h from defining them in wx/msw/wrapwin.h as they conflict
// with standard functions of the same name elsewhere, so we have to pay for it
// by manually redefining them ourselves here
#ifndef max
    #define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
    #define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#include <gdiplus.h>
using namespace Gdiplus;

#endif // _WX_MSW_WRAPGDIP_H_

