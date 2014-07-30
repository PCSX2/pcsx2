/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/private.h
// Purpose:     Private declarations for X11 port
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_H_
#define _WX_PRIVATE_H_

#include "wx/defs.h"
#include "wx/hashmap.h"
#include "wx/utils.h"
#if defined( __cplusplus ) && defined( __VMS )
#pragma message disable nosimpint
#endif
#include "X11/Xlib.h"
#include "X11/Xatom.h"
#include "X11/Xutil.h"
#if defined( __cplusplus ) && defined( __VMS )
#pragma message enable nosimpint
#endif

// Include common declarations
#include "wx/x11/privx.h"

#if wxUSE_PANGO
#include <pango/pango.h>
#endif

class WXDLLIMPEXP_FWD_CORE wxMouseEvent;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;
class WXDLLIMPEXP_FWD_CORE wxWindow;

// ----------------------------------------------------------------------------
// Some Unicode <-> UTF8 macros stolen from GTK
// ----------------------------------------------------------------------------

#if wxUSE_UNICODE
    #define wxGTK_CONV(s) wxConvUTF8.cWX2MB(s)
    #define wxGTK_CONV_BACK(s) wxConvUTF8.cMB2WX(s)
#else
    #define wxGTK_CONV(s) s.c_str()
    #define wxGTK_CONV_BACK(s) s
#endif

// ----------------------------------------------------------------------------
// we maintain a hash table which contains the mapping from Widget to wxWindow
// corresponding to the window for this widget
// ----------------------------------------------------------------------------

WX_DECLARE_HASH_MAP(Window, wxWindow *, wxIntegerHash, wxIntegerEqual, wxWindowHash);

// these hashes are defined in app.cpp
extern wxWindowHash *wxWidgetHashTable;
extern wxWindowHash *wxClientWidgetHashTable;

extern void wxDeleteWindowFromTable(Window w);
extern wxWindow *wxGetWindowFromTable(Window w);
extern bool wxAddWindowToTable(Window w, wxWindow *win);

extern void wxDeleteClientWindowFromTable(Window w);
extern wxWindow *wxGetClientWindowFromTable(Window w);
extern bool wxAddClientWindowToTable(Window w, wxWindow *win);

// ----------------------------------------------------------------------------
// TranslateXXXEvent() functions - translate X event to wxWindow one
// ----------------------------------------------------------------------------
extern bool wxTranslateMouseEvent(wxMouseEvent& wxevent, wxWindow *win, Window window, XEvent *xevent);
extern bool wxTranslateKeyEvent(wxKeyEvent& wxevent, wxWindow *win, Window window, XEvent *xevent, bool isAscii = FALSE);

extern Window wxGetWindowParent(Window window);

// Set the window manager decorations according to the
// given wxWidgets style
bool wxSetWMDecorations(Window w, long style);
bool wxMWMIsRunning(Window w);

#endif
// _WX_PRIVATE_H_
