/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/app.h
// Purpose:     wxApp class
// Author:      David Elliott
// Modified by:
// Created:     2002/11/27
// Copyright:   (c) 2002 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_APP_H_
#define _WX_COCOA_APP_H_

typedef struct __CFRunLoopObserver * CFRunLoopObserverRef;
typedef const struct __CFString * CFStringRef;

#include "wx/osx/core/cfref.h"

// ========================================================================
// wxApp
// ========================================================================
// Represents the application. Derive OnInit and declare
// a new App object to start application
class WXDLLIMPEXP_CORE wxApp: public wxAppBase
{
    DECLARE_DYNAMIC_CLASS(wxApp)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxApp();
    virtual ~wxApp();

// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
public:
    inline WX_NSApplication GetNSApplication() { return m_cocoaApp; }
    virtual void CocoaDelegate_applicationWillBecomeActive();
    virtual void CocoaDelegate_applicationDidBecomeActive();
    virtual void CocoaDelegate_applicationWillResignActive();
    virtual void CocoaDelegate_applicationDidResignActive();
    virtual void CocoaDelegate_applicationWillUpdate();
    virtual void CF_ObserveMainRunLoopBeforeWaiting(CFRunLoopObserverRef observer, int activity);
protected:
    WX_NSApplication m_cocoaApp;
    struct objc_object *m_cocoaAppDelegate;
    WX_NSThread m_cocoaMainThread;
    wxCFRef<CFRunLoopObserverRef> m_cfRunLoopIdleObserver;
    wxCFRef<CFStringRef> m_cfObservedRunLoopMode;

// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // Implement wxAppBase pure virtuals
    virtual void Exit();

    virtual void WakeUpIdle();

    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp();
    virtual bool CallOnInit();


    virtual bool OnInit();
    virtual bool OnInitGui();

    // Set true _before_ initializing wx to force embedded mode (no app delegate, etc.)
    static bool sm_isEmbedded;
};

#endif // _WX_COCOA_APP_H_
