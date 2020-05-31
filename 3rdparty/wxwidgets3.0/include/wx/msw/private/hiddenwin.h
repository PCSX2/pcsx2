///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private/hiddenwin.h
// Purpose:     Helper for creating a hidden window used by wxMSW internally.
// Author:      Vadim Zeitlin
// Created:     2011-09-16
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_PRIVATE_HIDDENWIN_H_
#define _WX_MSW_PRIVATE_HIDDENWIN_H_

#include "wx/msw/private.h"

/*
  Creates a hidden window with supplied window proc registering the class for
  it if necessary (i.e. the first time only). Caller is responsible for
  destroying the window and unregistering the class (note that this must be
  done because wxWidgets may be used as a DLL and so may be loaded/unloaded
  multiple times into/from the same process so we can't rely on automatic
  Windows class unregistration).

  pclassname is a pointer to a caller stored classname, which must initially be
  NULL. classname is the desired wndclass classname. If function successfully
  registers the class, pclassname will be set to classname.
 */
extern "C" WXDLLIMPEXP_BASE HWND
wxCreateHiddenWindow(LPCTSTR *pclassname, LPCTSTR classname, WNDPROC wndproc);

#endif // _WX_MSW_PRIVATE_HIDDENWIN_H_
