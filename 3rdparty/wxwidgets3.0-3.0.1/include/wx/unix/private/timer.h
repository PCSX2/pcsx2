/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/timer.h
// Purpose:     wxTimer for wxBase (unix)
// Author:      Lukasz Michalski
// Created:     15/01/2005
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_PRIVATE_TIMER_H_
#define _WX_UNIX_PRIVATE_TIMER_H_

#if wxUSE_TIMER

#include "wx/private/timer.h"

// the type used for milliseconds is large enough for microseconds too but
// introduce a synonym for it to avoid confusion
typedef wxMilliClock_t wxUsecClock_t;

// ----------------------------------------------------------------------------
// wxTimer implementation class for Unix platforms
// ----------------------------------------------------------------------------

// NB: we have to export at least this symbol from the shared library, because
//     it's used by wxDFB's wxCore
class WXDLLIMPEXP_BASE wxUnixTimerImpl : public wxTimerImpl
{
public:
    wxUnixTimerImpl(wxTimer *timer);
    virtual ~wxUnixTimerImpl();

    virtual bool IsRunning() const;
    virtual bool Start(int milliseconds = -1, bool oneShot = false);
    virtual void Stop();

    // for wxTimerScheduler only: resets the internal flag indicating that the
    // timer is running
    void MarkStopped()
    {
        wxASSERT_MSG( m_isRunning, wxT("stopping non-running timer?") );

        m_isRunning = false;
    }

private:
    bool m_isRunning;
};

// ----------------------------------------------------------------------------
// wxTimerSchedule: information about a single timer, used by wxTimerScheduler
// ----------------------------------------------------------------------------

struct wxTimerSchedule
{
    wxTimerSchedule(wxUnixTimerImpl *timer, wxUsecClock_t expiration)
        : m_timer(timer),
          m_expiration(expiration)
    {
    }

    // the timer itself (we don't own this pointer)
    wxUnixTimerImpl *m_timer;

    // the time of its next expiration, in usec
    wxUsecClock_t m_expiration;
};

// the linked list of all active timers, we keep it sorted by expiration time
WX_DECLARE_LIST(wxTimerSchedule, wxTimerList);

// ----------------------------------------------------------------------------
// wxTimerScheduler: class responsible for updating all timers
// ----------------------------------------------------------------------------

class wxTimerScheduler
{
public:
    // get the unique timer scheduler instance
    static wxTimerScheduler& Get()
    {
        if ( !ms_instance )
            ms_instance = new wxTimerScheduler;

        return *ms_instance;
    }

    // must be called on shutdown to delete the global timer scheduler
    static void Shutdown()
    {
        if ( ms_instance )
        {
            delete ms_instance;
            ms_instance = NULL;
        }
    }

    // adds timer which should expire at the given absolute time to the list
    void AddTimer(wxUnixTimerImpl *timer, wxUsecClock_t expiration);

    // remove timer from the list, called automatically from timer dtor
    void RemoveTimer(wxUnixTimerImpl *timer);


    // the functions below are used by the event loop implementation to monitor
    // and notify timers:

    // if this function returns true, the time remaining until the next time
    // expiration is returned in the provided parameter (always positive or 0)
    //
    // it returns false if there are no timers
    bool GetNext(wxUsecClock_t *remaining) const;

    // trigger the timer event for all timers which have expired, return true
    // if any did
    bool NotifyExpired();

private:
    // ctor and dtor are private, this is a singleton class only created by
    // Get() and destroyed by Shutdown()
    wxTimerScheduler() { }
    ~wxTimerScheduler();

    // add the given timer schedule to the list in the right place
    //
    // we take ownership of the pointer "s" which must be heap-allocated
    void DoAddTimer(wxTimerSchedule *s);


    // the list of all currently active timers sorted by expiration
    wxTimerList m_timers;

    static wxTimerScheduler *ms_instance;
};

#endif // wxUSE_TIMER

#endif // _WX_UNIX_PRIVATE_TIMER_H_
