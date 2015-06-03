/////////////////////////////////////////////////////////////////////////////
// Name:        wx/timer.h
// Purpose:     wxTimer, wxStopWatch and global time-related functions
// Author:      Julian Smart
// Modified by: Vadim Zeitlin (wxTimerBase)
//              Guillermo Rodriguez (global clean up)
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TIMER_H_BASE_
#define _WX_TIMER_H_BASE_

#include "wx/defs.h"

#if wxUSE_TIMER

#include "wx/object.h"
#include "wx/longlong.h"
#include "wx/event.h"
#include "wx/stopwatch.h" // for backwards compatibility
#include "wx/utils.h"


// more readable flags for Start():

// generate notifications periodically until the timer is stopped (default)
#define wxTIMER_CONTINUOUS false

// only send the notification once and then stop the timer
#define wxTIMER_ONE_SHOT true

class WXDLLIMPEXP_FWD_BASE wxTimerImpl;
class WXDLLIMPEXP_FWD_BASE wxTimerEvent;

// timer event type
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_BASE, wxEVT_TIMER, wxTimerEvent);

// the interface of wxTimer class
class WXDLLIMPEXP_BASE wxTimer : public wxEvtHandler
{
public:
    // ctors and initializers
    // ----------------------

    // default: if you don't call SetOwner(), your only chance to get timer
    // notifications is to override Notify() in the derived class
    wxTimer()
    {
        Init();
        SetOwner(this);
    }

    // ctor which allows to avoid having to override Notify() in the derived
    // class: the owner will get timer notifications which can be handled with
    // EVT_TIMER
    wxTimer(wxEvtHandler *owner, int timerid = wxID_ANY)
    {
        Init();
        SetOwner(owner, timerid);
    }

    // same as ctor above
    void SetOwner(wxEvtHandler *owner, int timerid = wxID_ANY);

    virtual ~wxTimer();


    // working with the timer
    // ----------------------

    // NB: Start() and Stop() are not supposed to be overridden, they are only
    //     virtual for historical reasons, only Notify() can be overridden

    // start the timer: if milliseconds == -1, use the same value as for the
    // last Start()
    //
    // it is now valid to call Start() multiple times: this just restarts the
    // timer if it is already running
    virtual bool Start(int milliseconds = -1, bool oneShot = false);

    // start the timer for one iteration only, this is just a simple wrapper
    // for Start()
    bool StartOnce(int milliseconds = -1) { return Start(milliseconds, true); }

    // stop the timer, does nothing if the timer is not running
    virtual void Stop();

    // override this in your wxTimer-derived class if you want to process timer
    // messages in it, use non default ctor or SetOwner() otherwise
    virtual void Notify();


    // accessors
    // ---------

    // get the object notified about the timer events
    wxEvtHandler *GetOwner() const;

    // return true if the timer is running
    bool IsRunning() const;

    // return the timer ID
    int GetId() const;

    // get the (last) timer interval in milliseconds
    int GetInterval() const;

    // return true if the timer is one shot
    bool IsOneShot() const;

protected:
    // common part of all ctors
    void Init();

    wxTimerImpl *m_impl;

    wxDECLARE_NO_COPY_CLASS(wxTimer);
};

// ----------------------------------------------------------------------------
// wxTimerRunner: starts the timer in its ctor, stops in the dtor
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxTimerRunner
{
public:
    wxTimerRunner(wxTimer& timer) : m_timer(timer) { }
    wxTimerRunner(wxTimer& timer, int milli, bool oneShot = false)
        : m_timer(timer)
    {
        m_timer.Start(milli, oneShot);
    }

    void Start(int milli, bool oneShot = false)
    {
        m_timer.Start(milli, oneShot);
    }

    ~wxTimerRunner()
    {
        if ( m_timer.IsRunning() )
        {
            m_timer.Stop();
        }
    }

private:
    wxTimer& m_timer;

    wxDECLARE_NO_COPY_CLASS(wxTimerRunner);
};

// ----------------------------------------------------------------------------
// wxTimerEvent
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxTimerEvent : public wxEvent
{
public:
    wxTimerEvent()
        : wxEvent(wxID_ANY, wxEVT_TIMER) { m_timer=NULL; }

    wxTimerEvent(wxTimer& timer)
        : wxEvent(timer.GetId(), wxEVT_TIMER),
          m_timer(&timer)
    {
        SetEventObject(timer.GetOwner());
    }

    // accessors
    int GetInterval() const { return m_timer->GetInterval(); }
    wxTimer& GetTimer() const { return *m_timer; }

    // implement the base class pure virtual
    virtual wxEvent *Clone() const wxOVERRIDE { return new wxTimerEvent(*this); }
    virtual wxEventCategory GetEventCategory() const wxOVERRIDE { return wxEVT_CATEGORY_TIMER; }

private:
    wxTimer* m_timer;

    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxTimerEvent);
};

typedef void (wxEvtHandler::*wxTimerEventFunction)(wxTimerEvent&);

#define wxTimerEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxTimerEventFunction, func)

#define EVT_TIMER(timerid, func) \
    wx__DECLARE_EVT1(wxEVT_TIMER, timerid, wxTimerEventHandler(func))

#endif // wxUSE_TIMER

#endif // _WX_TIMER_H_BASE_
