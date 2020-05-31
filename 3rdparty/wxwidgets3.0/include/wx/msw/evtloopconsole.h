///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/evtloopconsole.h
// Purpose:     wxConsoleEventLoop class for Windows
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-07-31
// Copyright:   (c) 2003-2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_EVTLOOPCONSOLE_H_
#define _WX_MSW_EVTLOOPCONSOLE_H_

class WXDLLIMPEXP_BASE wxMSWEventLoopBase : public wxEventLoopManual
{
public:
    wxMSWEventLoopBase();

    // implement base class pure virtuals
    virtual bool Pending() const;

protected:
    // get the next message from queue and return true or return false if we
    // got WM_QUIT or an error occurred
    bool GetNextMessage(WXMSG *msg);

    // same as above but with a timeout and return value can be -1 meaning that
    // time out expired in addition to
    int GetNextMessageTimeout(WXMSG *msg, unsigned long timeout);
};

#if wxUSE_CONSOLE_EVENTLOOP

class WXDLLIMPEXP_BASE wxConsoleEventLoop : public wxMSWEventLoopBase
{
public:
    wxConsoleEventLoop() { }

    // override/implement base class virtuals
    virtual bool Dispatch();
    virtual int DispatchTimeout(unsigned long timeout);
    virtual void WakeUp();
    virtual bool YieldFor(long WXUNUSED(eventsToProcess)) { return true; }

    // Windows-specific function to process a single message
    virtual void ProcessMessage(WXMSG *msg);
};

#endif // wxUSE_CONSOLE_EVENTLOOP

#endif // _WX_MSW_EVTLOOPCONSOLE_H_
