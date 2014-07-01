///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wrapcdlg.h
// Purpose:     Wrapper for the standard <commdlg.h> header
// Author:      Wlodzimierz ABX Skiba
// Modified by:
// Created:     22.03.2005
// Copyright:   (c) 2005 Wlodzimierz Skiba
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_WRAPCDLG_H_
#define _WX_MSW_WRAPCDLG_H_

#include "wx/defs.h"

#include "wx/msw/wrapwin.h"
#include "wx/msw/private.h"
#include "wx/msw/missing.h"

#if wxUSE_COMMON_DIALOGS && !defined(__SMARTPHONE__) && !defined(__WXMICROWIN__)
    #include <commdlg.h>
#endif

#include "wx/msw/winundef.h"

#endif // _WX_MSW_WRAPCDLG_H_

