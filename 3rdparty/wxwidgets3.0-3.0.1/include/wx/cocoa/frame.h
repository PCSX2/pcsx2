/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/frame.h
// Purpose:     wxFrame class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_FRAME_H_
#define _WX_COCOA_FRAME_H_

class WXDLLIMPEXP_FWD_CORE wxMenuBar;
class WXDLLIMPEXP_FWD_CORE wxStatusBar;

class WXDLLIMPEXP_CORE wxFrame: public wxFrameBase
{
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxFrame)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxFrame() { Init(); }
    wxFrame(wxWindow *parent,
            wxWindowID winid,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE,
            const wxString& name = wxFrameNameStr)
    {
        Init();
        Create(parent, winid, title, pos, size, style, name);
    }

    virtual ~wxFrame();

    bool Create(wxWindow *parent,
                wxWindowID winid,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);
protected:
    void Init();
// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
public:
    virtual wxMenuBar* GetAppMenuBar(wxCocoaNSWindow *win);
    // Returns the NSView for non-client drawing
    virtual WX_NSView GetNonClientNSView();

    // Helper function to position status/tool bars
    // Also called by native toolbar to force a size update
    void UpdateFrameNSView();

    virtual void CocoaDelegate_wxMenuItemAction(WX_NSMenuItem menuItem);
    virtual bool CocoaDelegate_validateMenuItem(WX_NSMenuItem menuItem);
protected:
    virtual void CocoaSetWxWindowSize(int width, int height);

    virtual void CocoaReplaceView(WX_NSView oldView, WX_NSView newView);
    // frameNSView is used whenever a statusbar/generic toolbar are present
    WX_NSView m_frameNSView;
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual void AttachMenuBar(wxMenuBar *mbar);
    virtual void DetachMenuBar();
    virtual void SetMenuBar(wxMenuBar *menubar);

    // implementation only from now on
    // -------------------------------

    // override some more virtuals

    // get the origin of the client area (which may be different from (0, 0)
    // if the frame has a toolbar) in client coordinates
    virtual wxPoint GetClientAreaOrigin() const;

    // create the main status bar by calling OnCreateStatusBar()
    virtual wxStatusBar* CreateStatusBar(int number = 1,
                                         long style = wxSTB_DEFAULT_STYLE,
                                         wxWindowID winid = 0,
                                         const wxString& name =
                                            wxStatusLineNameStr);
    // sets the main status bar
    void SetStatusBar(wxStatusBar *statBar);
#if wxUSE_TOOLBAR
    // create main toolbar bycalling OnCreateToolBar()
    virtual wxToolBar* CreateToolBar(long style = -1,
                                     wxWindowID winid = wxID_ANY,
                                     const wxString& name = wxToolBarNameStr);
    // sets the main tool bar
    virtual void SetToolBar(wxToolBar *toolbar);
#endif //wxUSE_TOOLBAR
protected:
    void PositionStatusBar();
};

#endif // _WX_COCOA_FRAME_H_
