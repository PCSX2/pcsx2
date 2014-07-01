/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/reparent.h
// Purpose:     Reparenting classes
// Author:      Julian Smart
// Modified by:
// Created:     2002-03-09
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_REPARENT_H_
#define _WX_REPARENT_H_

#include "wx/window.h"

/*
 * This class helps to reparent a specific window
 */

class WXDLLIMPEXP_FWD_CORE wxAdoptedWindow;
class WXDLLIMPEXP_CORE wxReparenter: public wxObject
{
public:
    wxReparenter() {}

    // We assume that toReparent has had its X window set
    // appropriately. toReparent is typically a wxAdoptedWindow.
    bool Reparent(wxWindow* newParent, wxAdoptedWindow* toReparent);

    // Wait for an appropriate window to be created.
    // If exactMatch is FALSE, a substring match is OK.
    // If windowName is empty, then wait for the next overrideRedirect window.
    bool WaitAndReparent(wxWindow* newParent, wxAdoptedWindow* toReparent,
                         const wxString& windowName = wxEmptyString,
                         bool exactMatch = TRUE);

protected:

    bool ProcessXEvent(WXEvent* event);
    WXWindow FindAClientWindow(WXWindow window, const wxString& name);

    static bool sm_done;
    static wxAdoptedWindow* sm_toReparent;
    static wxWindow* sm_newParent;
    static wxString  sm_name;
    static bool      sm_exactMatch;
};

/*
 * A window that adopts its handle from the native
 * toolkit. It has no parent until reparented.
 */

class WXDLLIMPEXP_CORE wxAdoptedWindow: public wxWindow
{
  public:
    wxAdoptedWindow();
    wxAdoptedWindow(WXWindow window);
    virtual ~wxAdoptedWindow();

    void SetHandle(WXWindow window) { m_mainWindow = window; m_clientWindow = window; }
    WXWindow GetHandle() const { return X11GetMainWindow(); }
};

#endif
// _WX_REPARENT_H_
