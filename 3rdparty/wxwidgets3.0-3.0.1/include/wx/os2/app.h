/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/app.h
// Purpose:     wxApp class
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_APP_H_
#define _WX_APP_H_

#ifdef __WATCOMC__

#include <types.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#else

#include <sys/time.h>
#include <sys/types.h>

#ifdef __EMX__
#include <unistd.h>
#else
#include <utils.h>
#undef BYTE_ORDER
#include <types.h>
#define INCL_ORDERS
#endif

#endif

#include "wx/event.h"
#include "wx/icon.h"

class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxApp;
class WXDLLIMPEXP_FWD_CORE wxKeyEvent;
class WXDLLIMPEXP_FWD_BASE wxLog;

WXDLLIMPEXP_DATA_CORE(extern wxApp*) wxTheApp;
WXDLLIMPEXP_DATA_CORE(extern HAB)    vHabmain;

// Force an exit from main loop
void WXDLLIMPEXP_CORE wxExit(void);

// Yield to other apps/messages
bool WXDLLIMPEXP_CORE wxYield(void);

extern MRESULT EXPENTRY wxWndProc( HWND
                                  ,ULONG
                                  ,MPARAM
                                  ,MPARAM
                                 );


// Represents the application. Derive OnInit and declare
// a new App object to start application
class WXDLLIMPEXP_CORE wxApp : public wxAppBase
{
    DECLARE_DYNAMIC_CLASS(wxApp)

public:
    wxApp();
    virtual ~wxApp();

    // override base class (pure) virtuals
    virtual bool Initialize(int& argc, wxChar **argv);
    virtual void CleanUp(void);

    virtual bool OnInitGui(void);

    virtual void WakeUpIdle(void);

    virtual void SetPrintMode(int mode) { m_nPrintMode = mode; }
    virtual int  GetPrintMode(void) const { return m_nPrintMode; }

    // implementation only
    void OnIdle(wxIdleEvent& rEvent);
    void OnEndSession(wxCloseEvent& rEvent);
    void OnQueryEndSession(wxCloseEvent& rEvent);

    int AddSocketHandler(int handle, int mask,
                         void (*callback)(void*), void * gsock);
    void RemoveSocketHandler(int handle);
    void HandleSockets();

protected:
    bool                            m_bShowOnInit;
    int                             m_nPrintMode; // wxPRINT_WINDOWS, wxPRINT_POSTSCRIPT

    //
    // PM-specific wxApp definitions */
    //
private:
    int                             m_maxSocketHandles;
    int                             m_maxSocketNr;
    int                             m_lastUsedHandle;
    fd_set                          m_readfds;
    fd_set                          m_writefds;
    void*                           m_sockCallbackInfo;

public:

    // Implementation
    static bool  RegisterWindowClasses(HAB vHab);

public:
    int                             m_nCmdShow;
    HMQ                             m_hMq;

protected:
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxApp);
};
#endif
    // _WX_APP_H_
