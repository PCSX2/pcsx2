/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/app.h
// Purpose:     wxApp class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_APP_H_
#define _WX_APP_H_

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/gdicmn.h"
#include "wx/event.h"

class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxWindowMac;
class WXDLLIMPEXP_FWD_CORE wxApp ;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;
class WXDLLIMPEXP_FWD_BASE wxLog;
class WXDLLIMPEXP_FWD_CORE wxMacAutoreleasePool;

// Force an exit from main loop
void WXDLLIMPEXP_CORE wxExit();

// Yield to other apps/messages
bool WXDLLIMPEXP_CORE wxYield();

// Represents the application. Derive OnInit and declare
// a new App object to start application
class WXDLLIMPEXP_CORE wxApp: public wxAppBase
{
    DECLARE_DYNAMIC_CLASS(wxApp)

    wxApp();
    virtual ~wxApp();

    virtual void WakeUpIdle();

    virtual void SetPrintMode(int mode) { m_printMode = mode; }
    virtual int GetPrintMode() const { return m_printMode; }

    // calling OnInit with an auto-release pool ready ...
    virtual bool CallOnInit();
#if wxUSE_GUI
    // setting up all MacOS Specific Event-Handlers etc
    virtual bool OnInitGui();
#endif // wxUSE_GUI

    virtual int OnRun();

    virtual bool ProcessIdle();

    // implementation only
    void OnIdle(wxIdleEvent& event);
    void OnEndSession(wxCloseEvent& event);
    void OnQueryEndSession(wxCloseEvent& event);

protected:
    int                   m_printMode; // wxPRINT_WINDOWS, wxPRINT_POSTSCRIPT
    wxMacAutoreleasePool* m_macPool;

public:

    static bool           sm_isEmbedded;
    // Implementation
    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp();

    // the installed application event handler
    WXEVENTHANDLERREF    MacGetEventHandler() { return m_macEventHandler ; }
    WXEVENTHANDLERREF    MacGetCurrentEventHandlerCallRef() { return m_macCurrentEventHandlerCallRef ; }
    void MacSetCurrentEvent( WXEVENTREF event , WXEVENTHANDLERCALLREF handler )
    { m_macCurrentEvent = event ; m_macCurrentEventHandlerCallRef = handler ; }

    // adding a CFType object to be released only at the end of the current event cycle (increases the
    // refcount of the object passed), needed in case we are in the middle of an event concering an object
    // we want to delete and cannot do it immediately
    // TODO change semantics to be in line with cocoa (make autrelease NOT increase the count)
    void                  MacAddToAutorelease( void* cfrefobj );
    void                  MacReleaseAutoreleasePool();
    
public:
    static wxWindow*      s_captureWindow ;
    static long           s_lastModifiers ;

    int                   m_nCmdShow;

    // mac specifics
protected:
#if wxOSX_USE_COCOA
    // override for support of custom app controllers
    virtual WX_NSObject   OSXCreateAppController();
#endif
    
private:
    virtual bool        DoInitGui();
    virtual void        DoCleanUp();

    WXEVENTHANDLERREF     m_macEventHandler ;
    WXEVENTHANDLERCALLREF m_macCurrentEventHandlerCallRef ;
    WXEVENTREF            m_macCurrentEvent ;

public:
    static long           s_macAboutMenuItemId ;
    static long           s_macPreferencesMenuItemId ;
    static long           s_macExitMenuItemId ;
    static wxString       s_macHelpMenuTitleName ;

    WXEVENTREF            MacGetCurrentEvent() { return m_macCurrentEvent ; }

    // For embedded use. By default does nothing.
    virtual void          MacHandleUnhandledEvent( WXEVENTREF ev );

    bool    MacSendKeyDownEvent( wxWindow* focus , long keyval , long modifiers , long when , wxChar uniChar ) ;
    bool    MacSendKeyUpEvent( wxWindow* focus , long keyval , long modifiers , long when , wxChar uniChar ) ;
    bool    MacSendCharEvent( wxWindow* focus , long keymessage , long modifiers , long when , wxChar uniChar ) ;
    void    MacCreateKeyEvent( wxKeyEvent& event, wxWindow* focus , long keymessage , long modifiers , long when , wxChar uniChar ) ;
#if wxOSX_USE_CARBON
    // we only have applescript on these
    virtual short         MacHandleAEODoc(const WXAPPLEEVENTREF event , WXAPPLEEVENTREF reply) ;
    virtual short         MacHandleAEGURL(const WXAPPLEEVENTREF event , WXAPPLEEVENTREF reply) ;
    virtual short         MacHandleAEPDoc(const WXAPPLEEVENTREF event , WXAPPLEEVENTREF reply) ;
    virtual short         MacHandleAEOApp(const WXAPPLEEVENTREF event , WXAPPLEEVENTREF reply) ;
    virtual short         MacHandleAEQuit(const WXAPPLEEVENTREF event , WXAPPLEEVENTREF reply) ;
    virtual short         MacHandleAERApp(const WXAPPLEEVENTREF event , WXAPPLEEVENTREF reply) ;
#endif
    // in response of an openFiles message with Cocoa and an
    // open-document apple event
    virtual void         MacOpenFiles(const wxArrayString &fileNames) ;
    // called by MacOpenFiles for each file.
    virtual void         MacOpenFile(const wxString &fileName) ;
    // in response of a get-url apple event
    virtual void         MacOpenURL(const wxString &url) ;
    // in response of a print-document apple event
    virtual void         MacPrintFiles(const wxArrayString &fileNames) ;
    // called by MacPrintFiles for each file
    virtual void         MacPrintFile(const wxString &fileName) ;
    // in response of a open-application apple event
    virtual void         MacNewFile() ;
    // in response of a reopen-application apple event
    virtual void         MacReopenApp() ;

    // Notice that this is just a placeholder and doesn't work yet!
    //
    // Override this to return false from a non-bundled console app in order to
    // stay in background instead of being made a foreground application as
    // happens by default.
    virtual bool         OSXIsGUIApplication() { return true; }

#if wxOSX_USE_COCOA_OR_IPHONE
    // immediately before the native event loop launches
    virtual void         OSXOnWillFinishLaunching();
    // immediately when the native event loop starts, no events have been served yet
    virtual void         OSXOnDidFinishLaunching();
    // OS asks to terminate app, return no to stay running
    virtual bool         OSXOnShouldTerminate();
    // before application terminates
    virtual void         OSXOnWillTerminate();

private:
    bool                m_onInitResult;
    bool                m_inited;
    wxArrayString       m_openFiles;
    wxArrayString       m_printFiles;
    wxString            m_getURL;
    
public:
    bool                OSXInitWasCalled() { return m_inited; }
    void                OSXStoreOpenFiles(const wxArrayString &files ) { m_openFiles = files ; }
    void                OSXStorePrintFiles(const wxArrayString &files ) { m_printFiles = files ; }
    void                OSXStoreOpenURL(const wxString &url ) { m_getURL = url ; }
#endif
    
    // Hide the application windows the same as the system hide command would do it.
    void MacHideApp();

    DECLARE_EVENT_TABLE()
};

#endif
    // _WX_APP_H_

