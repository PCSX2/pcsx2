/////////////////////////////////////////////////////////////////////////////
// Name:        wx/wxprec.h
// Purpose:     Includes the appropriate files for precompiled headers
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// compiler detection; includes setup.h
#include "wx/defs.h"

// check if to use precompiled headers: do it for most Windows compilers unless
// explicitly disabled by defining NOPCH
#if defined(__VISUALC__)   || \
    defined(__DMC__)       || \
    defined(__VISAGECPP__) || \
    defined(__WATCOMC__)   || \
    defined(__BORLANDC__)


    // If user did not request NOCPH and we're not building using configure
    // then assume user wants precompiled headers.
    #if !defined(NOPCH) && !defined(__WX_SETUP_H__)
        #define WX_PRECOMP
    #endif
#endif

// For some reason, this must be defined for common dialogs to work.
#ifdef __WATCOMC__
    #define INCLUDE_COMMDLG_H  1
#endif

#ifdef WX_PRECOMP

// include "wx/chartype.h" first to ensure that UNICODE macro is correctly set
// _before_ including <windows.h>
#include "wx/chartype.h"

// include standard Windows headers
#if defined(__WINDOWS__)
    #include "wx/msw/wrapwin.h"
    #include "wx/msw/private.h"
#endif
#if defined(__WXMSW__)
    #include "wx/msw/wrapcctl.h"
    #include "wx/msw/wrapcdlg.h"
    #include "wx/msw/missing.h"
#endif

// include <os2.h>
#ifdef __OS2__
#   include "wx/os2/private.h"
#endif

// include the most common wx headers
#include "wx/wx.h"

#endif // WX_PRECOMP
