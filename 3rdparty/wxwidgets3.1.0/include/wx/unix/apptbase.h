///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/apptbase.h
// Purpose:     declaration of wxAppTraits for Unix systems
// Author:      Vadim Zeitlin
// Modified by:
// Created:     23.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_APPTBASE_H_
#define _WX_UNIX_APPTBASE_H_

#include "wx/evtloop.h"
#include "wx/evtloopsrc.h"

class wxExecuteData;
class wxFDIOManager;
class wxEventLoopSourcesManagerBase;

// ----------------------------------------------------------------------------
// wxAppTraits: the Unix version adds extra hooks needed by Unix code
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxAppTraits : public wxAppTraitsBase
{
public:
    // wxExecute() support methods
    // ---------------------------

    // Wait for the process termination and return its exit code or -1 on error.
    //
    // Notice that this is only used when execData.flags contains wxEXEC_SYNC
    // and does not contain wxEXEC_NOEVENTS, i.e. when we need to really wait
    // until the child process exit and dispatch the events while doing it.
    virtual int WaitForChild(wxExecuteData& execData);

#if wxUSE_SOCKETS
    // return a pointer to the object which should be used to integrate
    // monitoring of the file descriptors to the event loop (currently this is
    // used for the sockets only but should be used for arbitrary event loop
    // sources in the future)
    //
    // this object may be different for the console and GUI applications
    //
    // the pointer is not deleted by the caller as normally it points to a
    // static variable
    virtual wxFDIOManager *GetFDIOManager();
#endif // wxUSE_SOCKETS

#if wxUSE_EVENTLOOP_SOURCE
    // Return a non-NULL pointer to the object responsible for managing the
    // event loop sources in this kind of application.
    virtual wxEventLoopSourcesManagerBase* GetEventLoopSourcesManager();
#endif // wxUSE_CONSOLE_EVENTLOOP

protected:
    // Wait for the process termination by running the given event loop until
    // this happens.
    //
    // This is used by the public WaitForChild() after creating the event loop
    // of the appropriate kind.
    int RunLoopUntilChildExit(wxExecuteData& execData, wxEventLoopBase& loop);
};

#endif // _WX_UNIX_APPTBASE_H_

