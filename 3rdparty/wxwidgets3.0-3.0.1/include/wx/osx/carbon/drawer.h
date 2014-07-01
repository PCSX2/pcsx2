/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/drawer.h
// Purpose:     Drawer child window class.
//              Drawer windows appear under their parent window and
//              behave like a drawer, opening and closing to reveal
//              content that does not need to be visible at all times.
// Author:      Jason Bagley
// Modified by:
// Created:     2004-30-01
// Copyright:   (c) Jason Bagley; Art & Logic, Inc.
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DRAWERWINDOW_H_
#define _WX_DRAWERWINDOW_H_

#include "wx/toplevel.h"

//
// NB:  This is currently a private undocumented class -
// it is stable, but the API is not and will change in the
// near future
//

class WXDLLIMPEXP_ADV wxDrawerWindow : public wxTopLevelWindow
{
    DECLARE_DYNAMIC_CLASS(wxDrawerWindow)

public:

    wxDrawerWindow();

    wxDrawerWindow(wxWindow* parent,
                   wxWindowID id,
                   const wxString& title,
                   wxSize size = wxDefaultSize,
                   wxDirection edge = wxLEFT,
                   const wxString& name = wxT("drawerwindow"))
    {
        this->Create(parent, id, title, size, edge, name);
    }

    virtual ~wxDrawerWindow();

    // Create a drawer window.
    // If parent is NULL, create as a tool window.
    // If parent is not NULL, then wxTopLevelWindow::Attach this window to parent.
    bool Create(wxWindow *parent,
     wxWindowID id,
     const wxString& title,
     wxSize size = wxDefaultSize,
     wxDirection edge = wxLEFT,
     const wxString& name = wxFrameNameStr);

    bool Open(bool show = true); // open or close the drawer, possibility for async param, i.e. animate
    bool Close() { return this->Open(false); }
    bool IsOpen() const;

    // Set the edge of the parent where the drawer attaches.
    bool SetPreferredEdge(wxDirection edge);
    wxDirection GetPreferredEdge() const;
    wxDirection GetCurrentEdge() const; // not necessarily the preferred, due to screen constraints
};

#endif // _WX_DRAWERWINDOW_H_
