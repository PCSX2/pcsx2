/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/mdi.h
// Purpose:     wxMDIParentFrame, wxMDIChildFrame, wxMDIClientWindow
// Author:      David Elliott
// Modified by: 2008-10-31 Vadim Zeitlin: derive from the base classes
// Created:     2003/09/08
// Copyright:   (c) 2003 David Elliott
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_MDI_H__
#define __WX_COCOA_MDI_H__

#include "wx/frame.h"

DECLARE_WXCOCOA_OBJC_CLASS(wxMDIParentFrameObserver);

class WXDLLIMPEXP_FWD_CORE wxMDIChildFrame;
class WXDLLIMPEXP_FWD_CORE wxMDIClientWindow;

WX_DECLARE_EXPORTED_LIST(wxMDIChildFrame, wxCocoaMDIChildFrameList);

// ========================================================================
// wxMDIParentFrame
// ========================================================================
class WXDLLIMPEXP_CORE wxMDIParentFrame : public wxMDIParentFrameBase
{
    friend class WXDLLIMPEXP_FWD_CORE wxMDIChildFrame;
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxMDIParentFrame)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxMDIParentFrame() { Init(); }
    wxMDIParentFrame(wxWindow *parent,
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

    virtual ~wxMDIParentFrame();

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
    void WindowDidBecomeMain(NSNotification *notification);
protected:
    virtual void CocoaDelegate_windowDidBecomeKey(void);
    virtual void CocoaDelegate_windowDidResignKey(void);
    virtual bool Cocoa_canBecomeMainWindow(bool &canBecome);
    virtual wxMenuBar* GetAppMenuBar(wxCocoaNSWindow *win);

    void AddMDIChild(wxMDIChildFrame *child);
    void RemoveMDIChild(wxMDIChildFrame *child);

    wxMDIParentFrameObserver *m_observer;
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    void SetActiveChild(wxMDIChildFrame *child);

    // implement base class pure virtuals
    // ----------------------------------

    static bool IsTDI() { return false; }

    virtual void ActivateNext() { /* TODO */ }
    virtual void ActivatePrevious() { /* TODO */ }

protected:
    wxMDIClientWindow *m_clientWindow;
    wxMDIChildFrame *m_currentChild;
    wxCocoaMDIChildFrameList m_mdiChildren;
};

// ========================================================================
// wxMDIChildFrame
// ========================================================================
class WXDLLIMPEXP_CORE wxMDIChildFrame: public wxFrame
{
    friend class WXDLLIMPEXP_FWD_CORE wxMDIParentFrame;
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxMDIChildFrame)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxMDIChildFrame() { Init(); }
    wxMDIChildFrame(wxMDIParentFrame *parent,
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

    virtual ~wxMDIChildFrame();

    bool Create(wxMDIParentFrame *parent,
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
protected:
    virtual void CocoaDelegate_windowDidBecomeKey(void);
    virtual void CocoaDelegate_windowDidBecomeMain(void);
    virtual void CocoaDelegate_windowDidResignKey(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual void Activate();
    virtual bool Destroy();
protected:
    wxMDIParentFrame *m_mdiParent;
};

// ========================================================================
// wxMDIClientWindow
// ========================================================================
class wxMDIClientWindow : public wxMDIClientWindowBase
{
public:
    wxMDIClientWindow() { }

    virtual bool CreateClient(wxMDIParentFrame *parent,
                              long style = wxHSCROLL | wxVSCROLL);

    DECLARE_DYNAMIC_CLASS(wxMDIClientWindow)
};

#endif // __WX_COCOA_MDI_H__
