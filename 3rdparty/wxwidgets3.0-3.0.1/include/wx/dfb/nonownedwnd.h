///////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/nonownedwnd.h
// Purpose:     declares wxNonOwnedWindow class
// Author:      Vaclav Slavik
// Modified by:
// Created:     2006-12-24
// Copyright:   (c) 2006 TT-Solutions
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_NONOWNEDWND_H_
#define _WX_DFB_NONOWNEDWND_H_

#include "wx/window.h"
#include "wx/dfb/dfbptr.h"

wxDFB_DECLARE_INTERFACE(IDirectFBWindow);
class wxDfbQueuedPaintRequests;
struct wxDFBWindowEvent;
class wxDFBEventsHandler;

//-----------------------------------------------------------------------------
// wxNonOwnedWindow
//-----------------------------------------------------------------------------

// This class represents "non-owned" window. A window is owned by another
// window if it has a parent and is positioned within the parent. For example,
// wxFrame is non-owned, because even though it can have a parent, it's
// location is independent of it.  This class is for internal use only, it's
// the base class for wxTopLevelWindow and wxPopupWindow.
class WXDLLIMPEXP_CORE wxNonOwnedWindow : public wxNonOwnedWindowBase
{
public:
    // construction
    wxNonOwnedWindow() { Init(); }
    wxNonOwnedWindow(wxWindow *parent,
                     wxWindowID id,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = 0,
                     const wxString& name = wxPanelNameStr)
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxPanelNameStr);

    virtual ~wxNonOwnedWindow();

    // implement base class pure virtuals
    virtual bool Show(bool show = true);

    virtual void Update();

    virtual void Raise();
    virtual void Lower();

    // implementation from now on
    // --------------------------

    void OnInternalIdle();

    wxIDirectFBWindowPtr GetDirectFBWindow() const { return m_dfbwin; }

    // Returns true if some invalidated area of the TLW is currently being
    // painted
    bool IsPainting() const { return m_isPainting; }

protected:
    // common part of all ctors
    void Init();

    virtual wxIDirectFBSurfacePtr ObtainDfbSurface() const;

    // overridden wxWindow methods
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoMoveWindow(int x, int y, int width, int height);

    virtual void DoRefreshRect(const wxRect& rect);

    // sets DirectFB keyboard focus to this toplevel window (note that DFB
    // focus is different from wx: only shown TLWs can have it and not any
    // wxWindows as in wx
    void SetDfbFocus();

    // overridden in wxTopLevelWindowDFB, there's no common handling for wxTLW
    // and wxPopupWindow to be done here
    virtual void HandleFocusEvent(const wxDFBWindowEvent& WXUNUSED(event_)) {}

private:
    // do queued painting in idle time
    void HandleQueuedPaintRequests();

    // DirectFB events handling
    static void HandleDFBWindowEvent(const wxDFBWindowEvent& event_);

protected:
    // did we sent wxSizeEvent at least once?
    bool          m_sizeSet:1;

    // window's opacity (0: transparent, 255: opaque)
    wxByte        m_opacity;

    // interface to the underlying DirectFB window
    wxIDirectFBWindowPtr m_dfbwin;

private:
    // invalidated areas of the TLW that need repainting
    wxDfbQueuedPaintRequests *m_toPaint;
    // are we currently painting some area of this TLW?
    bool m_isPainting;

    friend class wxDFBEventsHandler; // for HandleDFBWindowEvent
    friend class wxWindowDFB;        // for SetDfbFocus
};

#endif // _WX_DFB_NONOWNEDWND_H_
