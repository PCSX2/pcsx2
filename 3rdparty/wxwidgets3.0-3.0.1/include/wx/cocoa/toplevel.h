///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/toplevel.h
// Purpose:     wxTopLevelWindowCocoa is the Cocoa implementation of wxTLW
// Author:      David Elliott
// Modified by:
// Created:     2002/12/08
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_TOPLEVEL_H__
#define __WX_COCOA_TOPLEVEL_H__

#include "wx/hashmap.h"
#include "wx/cocoa/NSWindow.h"

class WXDLLIMPEXP_FWD_CORE wxMenuBar;

// ========================================================================
// wxTopLevelWindowCocoa
// ========================================================================
class WXDLLIMPEXP_CORE wxTopLevelWindowCocoa : public wxTopLevelWindowBase, protected wxCocoaNSWindow
{
    DECLARE_EVENT_TABLE();
    DECLARE_NO_COPY_CLASS(wxTopLevelWindowCocoa);
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    inline wxTopLevelWindowCocoa()
    :   wxCocoaNSWindow(this)
    {   Init(); }

    inline wxTopLevelWindowCocoa(wxWindow *parent,
                        wxWindowID winid,
                        const wxString& title,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxDEFAULT_FRAME_STYLE,
                        const wxString& name = wxFrameNameStr)
    :   wxCocoaNSWindow(this)
    {
        Init();
        Create(parent, winid, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID winid,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxTopLevelWindowCocoa();

protected:
    // common part of all ctors
    void Init();

// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
public:
    inline WX_NSWindow GetNSWindow() { return m_cocoaNSWindow; }
    virtual void CocoaDelegate_windowWillClose(void);
    virtual bool CocoaDelegate_windowShouldClose(void);
    virtual void CocoaDelegate_windowDidBecomeKey(void);
    virtual void CocoaDelegate_windowDidResignKey(void);
    virtual void CocoaDelegate_windowDidBecomeMain(void);
    virtual void CocoaDelegate_windowDidResignMain(void);
    virtual void CocoaDelegate_wxMenuItemAction(WX_NSMenuItem sender);
    virtual bool CocoaDelegate_validateMenuItem(WX_NSMenuItem sender);
    virtual wxMenuBar* GetAppMenuBar(wxCocoaNSWindow *win);
    static void DeactivatePendingWindow();
protected:
    void SetNSWindow(WX_NSWindow cocoaNSWindow);
    WX_NSWindow m_cocoaNSWindow;
    static wxCocoaNSWindowHash sm_cocoaHash;
    virtual void CocoaReplaceView(WX_NSView oldView, WX_NSView newView);
    static unsigned int NSWindowStyleForWxStyle(long style);
    static NSRect MakeInitialNSWindowContentRect(const wxPoint& pos, const wxSize& size, unsigned int cocoaStyleMask);

    static wxTopLevelWindowCocoa *sm_cocoaDeactivateWindow;
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual bool Destroy();
    // Pure virtuals
    virtual void Maximize(bool maximize = true);
    virtual bool IsMaximized() const;
    virtual void Iconize(bool iconize = true);
    virtual bool IsIconized() const;
    virtual void Restore();
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL);
    virtual bool IsFullScreen() const;
    // other
    virtual bool Show( bool show = true );
    virtual bool Close( bool force = false );
    virtual void OnCloseWindow(wxCloseEvent& event);
    virtual void CocoaSetWxWindowSize(int width, int height);
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void SetTitle( const wxString& title);
    virtual wxString GetTitle() const;

    // Default button (item)
    wxWindow *SetDefaultItem(wxWindow *win);

// Things I may/may not do
//    virtual void SetIcons(const wxIconBundle& icons);
//    virtual void Clear() ;
//    virtual void Raise();
//    virtual void Lower();
protected:
    // is the frame currently iconized?
    bool m_iconized;
    // has the frame been closed
    bool m_closed;
    // should the frame be maximized when it will be shown? set by Maximize()
    // when it is called while the frame is hidden
    bool m_maximizeOnShow;
};

// list of all frames and modeless dialogs
extern WXDLLIMPEXP_DATA_CORE(wxWindowList) wxModelessWindows;

#endif // __WX_COCOA_TOPLEVEL_H__
