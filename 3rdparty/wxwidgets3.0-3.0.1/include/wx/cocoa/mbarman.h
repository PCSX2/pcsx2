///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/mbarman.h
// Purpose:     wxMenuBarManager class
// Author:      David Elliott
// Modified by:
// Created:     2003/09/04
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_MBARMAN_H__
#define __WX_COCOA_MBARMAN_H__

#if wxUSE_MENUS

#include "wx/toplevel.h"

DECLARE_WXCOCOA_OBJC_CLASS(wxMenuBarManagerObserver);

// ========================================================================
// wxMenuBarManager
// ========================================================================
class WXDLLIMPEXP_CORE wxMenuBarManager : public wxObject
{
// ------------------------------------------------------------------------
// initialization/destruction
// ------------------------------------------------------------------------
public:
    wxMenuBarManager();
    virtual ~wxMenuBarManager();
// ------------------------------------------------------------------------
// Single instance
// ------------------------------------------------------------------------
public:
    static wxMenuBarManager *GetInstance() { return sm_mbarmanInstance; }
    static void CreateInstance();
    static void DestroyInstance();
protected:
    static wxMenuBarManager *sm_mbarmanInstance;
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    void SetMainMenuBar(wxMenuBar* menubar);
    void WindowDidBecomeKey(NSNotification *notification);
#if 0
    void WindowDidResignKey(NSNotification *notification);
    void WindowDidBecomeMain(NSNotification *notification);
    void WindowDidResignMain(NSNotification *notification);
    void WindowWillClose(NSNotification *notification);
#endif // 0
    void UpdateMenuBar();
protected:
    void SetMenuBar(wxMenuBar* menubar);
    void InstallMenuBarForWindow(wxCocoaNSWindow *win);
    void InstallMainMenu();
    WX_NSMenu m_menuApp;
    WX_NSMenu m_menuServices;
    WX_NSMenu m_menuWindows;
    WX_NSMenu m_menuMain;
    // Is main menu bar the current one
    bool m_mainMenuBarInstalled;
    // Main menu (if app provides one)
    wxMenuBar *m_mainMenuBar;
    wxMenuBarManagerObserver *m_observer;
    WX_NSWindow m_currentNSWindow;
};

#endif // wxUSE_MENUS
#endif // _WX_COCOA_MBARMAN_H_
