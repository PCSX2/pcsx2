/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/frame.h
// Purpose:     wxFrame class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_
#define _WX_FRAME_H_

#include "wx/toolbar.h"
#include "wx/accel.h"
#include "wx/icon.h"

class WXDLLIMPEXP_FWD_CORE wxMacToolTip ;

class WXDLLIMPEXP_CORE wxFrame: public wxFrameBase
{
public:
    // construction
    wxFrame() { Init(); }
    wxFrame(wxWindow *parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE,
            const wxString& name = wxFrameNameStr)
    {
        Init();

        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxFrame();

    // implementation only from now on
    // -------------------------------

    // get the origin of the client area (which may be different from (0, 0)
    // if the frame has a toolbar) in client coordinates
    virtual wxPoint GetClientAreaOrigin() const;

    // override some more virtuals
    virtual bool Enable(bool enable = true) ;

    // event handlers
    void OnActivate(wxActivateEvent& event);
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    // Toolbar
#if wxUSE_TOOLBAR
    virtual wxToolBar* CreateToolBar(long style = -1,
                                     wxWindowID id = -1,
                                     const wxString& name = wxToolBarNameStr);

    virtual void SetToolBar(wxToolBar *toolbar);
#endif // wxUSE_TOOLBAR

    // Status bar
#if wxUSE_STATUSBAR
    virtual wxStatusBar* OnCreateStatusBar(int number = 1,
                                           long style = wxSTB_DEFAULT_STYLE,
                                           wxWindowID id = 0,
                                           const wxString& name = wxStatusLineNameStr);
#endif // wxUSE_STATUSBAR

    // called by wxWindow whenever it gets focus
    void SetLastFocus(wxWindow *win) { m_winLastFocused = win; }
    wxWindow *GetLastFocus() const { return m_winLastFocused; }

    void PositionBars();

    // internal response to size events
    virtual void MacOnInternalSize() { PositionBars(); }

protected:
    // common part of all ctors
    void Init();

#if wxUSE_TOOLBAR
    virtual void PositionToolBar();
#endif
#if wxUSE_STATUSBAR
    virtual void PositionStatusBar();
#endif

    // override base class virtuals
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoSetClientSize(int width, int height);

#if wxUSE_MENUS
    virtual void DetachMenuBar();
    virtual void AttachMenuBar(wxMenuBar *menubar);
#endif

    // the last focused child: we restore focus to it on activation
    wxWindow             *m_winLastFocused;

    virtual bool        MacIsChildOfClientArea( const wxWindow* child ) const ;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxFrame)
};

#endif
    // _WX_FRAME_H_
