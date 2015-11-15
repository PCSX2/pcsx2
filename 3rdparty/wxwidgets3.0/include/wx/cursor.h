/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cursor.h
// Purpose:     wxCursor base header
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CURSOR_H_BASE_
#define _WX_CURSOR_H_BASE_

#include "wx/defs.h"

/*
    wxCursor classes should have the following public API:

class WXDLLIMPEXP_CORE wxCursor : public wxGDIObject
{
public:
    wxCursor();
    wxCursor(const wxImage& image);
    wxCursor(const wxString& name,
             wxBitmapType type = wxCURSOR_DEFAULT_TYPE,
             int hotSpotX = 0, int hotSpotY = 0);
    wxCursor(wxStockCursor id) { InitFromStock(id); }
#if WXWIN_COMPATIBILITY_2_8
    wxCursor(int id) { InitFromStock((wxStockCursor)id); }
#endif
    virtual ~wxCursor();
};

*/

#if defined(__WXMSW__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_CUR_RESOURCE
    #include "wx/msw/cursor.h"
#elif defined(__WXMOTIF__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_XBM
    #include "wx/motif/cursor.h"
#elif defined(__WXGTK20__)
    #ifdef __WINDOWS__
        #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_CUR_RESOURCE
    #else
        #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_XPM
    #endif
    #include "wx/gtk/cursor.h"
#elif defined(__WXGTK__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_XPM
    #include "wx/gtk1/cursor.h"
#elif defined(__WXX11__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_XPM
    #include "wx/x11/cursor.h"
#elif defined(__WXDFB__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_CUR_RESOURCE
    #include "wx/dfb/cursor.h"
#elif defined(__WXMAC__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_MACCURSOR_RESOURCE
    #include "wx/osx/cursor.h"
#elif defined(__WXCOCOA__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_MACCURSOR_RESOURCE
    #include "wx/cocoa/cursor.h"
#elif defined(__WXPM__)
    #define wxCURSOR_DEFAULT_TYPE   wxBITMAP_TYPE_CUR_RESOURCE
    #include "wx/os2/cursor.h"
#endif

#include "wx/utils.h"

/* This is a small class which can be used by all ports
   to temporarily suspend the busy cursor. Useful in modal
   dialogs.

   Actually that is not (any longer) quite true..  currently it is
   only used in wxGTK Dialog::ShowModal() and now uses static
   wxBusyCursor methods that are only implemented for wxGTK so far.
   The BusyCursor handling code should probably be implemented in
   common code somewhere instead of the separate implementations we
   currently have.  Also the name BusyCursorSuspender is a little
   misleading since it doesn't actually suspend the BusyCursor, just
   masks one that is already showing.
   If another call to wxBeginBusyCursor is made while this is active
   the Busy Cursor will again be shown.  But at least now it doesn't
   interfere with the state of wxIsBusy() -- RL

*/
class wxBusyCursorSuspender
{
public:
    wxBusyCursorSuspender()
    {
        if( wxIsBusy() )
        {
            wxSetCursor( wxBusyCursor::GetStoredCursor() );
        }
    }
    ~wxBusyCursorSuspender()
    {
        if( wxIsBusy() )
        {
            wxSetCursor( wxBusyCursor::GetBusyCursor() );
        }
    }
};
#endif
    // _WX_CURSOR_H_BASE_
