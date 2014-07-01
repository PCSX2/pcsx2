///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/evtloop.h
// Purpose:     CoreFoundation-based event loop
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2006-01-12
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_CORE_EVTLOOP_H_
#define _WX_OSX_CORE_EVTLOOP_H_

DECLARE_WXOSX_OPAQUE_CFREF( CFRunLoop )
DECLARE_WXOSX_OPAQUE_CFREF( CFRunLoopObserver )

class WXDLLIMPEXP_FWD_BASE wxCFEventLoopPauseIdleEvents;

class WXDLLIMPEXP_BASE wxCFEventLoop : public wxEventLoopBase
{
    friend class wxCFEventLoopPauseIdleEvents;
public:
    wxCFEventLoop();
    virtual ~wxCFEventLoop();

    // sets the "should exit" flag and wakes up the loop so that it terminates
    // soon
    virtual void ScheduleExit(int rc = 0);

    // return true if any events are available
    virtual bool Pending() const;

    // dispatch a single event, return false if we should exit from the loop
    virtual bool Dispatch();

    // same as Dispatch() but doesn't wait for longer than the specified (in
    // ms) timeout, return true if an event was processed, false if we should
    // exit the loop or -1 if timeout expired
    virtual int DispatchTimeout(unsigned long timeout);

    // implement this to wake up the loop: usually done by posting a dummy event
    // to it (can be called from non main thread)
    virtual void WakeUp();

    virtual bool YieldFor(long eventsToProcess);

    bool ShouldProcessIdleEvents() const { return m_processIdleEvents ; }
    
#if wxUSE_UIACTIONSIMULATOR
    // notifies Yield and Dispatch to wait for at least one event before
    // returning, this is necessary, because the synthesized events need to be
    // converted by the OS before being available on the native event queue
    void SetShouldWaitForEvent(bool should) { m_shouldWaitForEvent = should; }
#endif
protected:
    // enters a loop calling OnNextIteration(), Pending() and Dispatch() and
    // terminating when Exit() is called
    virtual int DoRun();

    void CommonModeObserverCallBack(CFRunLoopObserverRef observer, int activity);
    void DefaultModeObserverCallBack(CFRunLoopObserverRef observer, int activity);

    // set to false to avoid idling at unexpected moments - eg when having native message boxes
    void SetProcessIdleEvents(bool process) { m_processIdleEvents = process; }

    static void OSXCommonModeObserverCallBack(CFRunLoopObserverRef observer, int activity, void *info);
    static void OSXDefaultModeObserverCallBack(CFRunLoopObserverRef observer, int activity, void *info);

    // get the currently executing CFRunLoop
    virtual CFRunLoopRef CFGetCurrentRunLoop() const;

    virtual int DoDispatchTimeout(unsigned long timeout);

    virtual void OSXDoRun();
    virtual void OSXDoStop();

    // the loop exit code
    int m_exitcode;

    // cfrunloop
    CFRunLoopRef m_runLoop;

    // common modes runloop observer
    CFRunLoopObserverRef m_commonModeRunLoopObserver;

    // default mode runloop observer
    CFRunLoopObserverRef m_defaultModeRunLoopObserver;

    // set to false to avoid idling at unexpected moments - eg when having native message boxes
    bool m_processIdleEvents;

#if wxUSE_UIACTIONSIMULATOR
    bool m_shouldWaitForEvent;
#endif
private:
    // process all already pending events and dispatch a new one (blocking
    // until it appears in the event queue if necessary)
    //
    // returns the return value of DoDispatchTimeout()
    int DoProcessEvents();

    wxDECLARE_NO_COPY_CLASS(wxCFEventLoop);
};

class WXDLLIMPEXP_BASE wxCFEventLoopPauseIdleEvents : public wxObject
{
public:
    wxCFEventLoopPauseIdleEvents();
    virtual ~wxCFEventLoopPauseIdleEvents();
private:
    bool m_formerState;
};

#endif // _WX_OSX_EVTLOOP_H_
