/////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/timer.h
// Purpose:     Base class for wxTimer implementations
// Author:      Lukasz Michalski <lmichalski@sf.net>
// Created:     31.10.2006
// Copyright:   (c) 2006-2007 wxWidgets dev team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TIMERIMPL_H_BASE_
#define _WX_TIMERIMPL_H_BASE_

#include "wx/defs.h"
#include "wx/event.h"
#include "wx/timer.h"

// ----------------------------------------------------------------------------
// wxTimerImpl: abstract base class for wxTimer implementations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxTimerImpl
{
public:
    // default ctor, SetOwner() must be called after it (wxTimer does it)
    wxTimerImpl(wxTimer *owner);

    // this must be called initially but may be also called later
    void SetOwner(wxEvtHandler *owner, int timerid);

    // empty but virtual base class dtor, the caller is responsible for
    // stopping the timer before it's destroyed (it can't be done from here as
    // it's too late)
    virtual ~wxTimerImpl() { }


    // start the timer. When overriding call base version first.
    virtual bool Start(int milliseconds = -1, bool oneShot = false);

    // stop the timer, only called if the timer is really running (unlike
    // wxTimer::Stop())
    virtual void Stop() = 0;

    // return true if the timer is running
    virtual bool IsRunning() const = 0;

    // this should be called by the port-specific code when the timer expires
    virtual void Notify() { m_timer->Notify(); }

    // the default implementation of wxTimer::Notify(): generate a wxEVT_TIMER
    void SendEvent();


    // accessors for wxTimer:
    wxEvtHandler *GetOwner() const { return m_owner; }
    int GetId() const { return m_idTimer; }
    int GetInterval() const { return m_milli; }
    bool IsOneShot() const { return m_oneShot; }

protected:
    wxTimer *m_timer;

    wxEvtHandler *m_owner;

    int     m_idTimer;      // id passed to wxTimerEvent
    int     m_milli;        // the timer interval
    bool    m_oneShot;      // true if one shot


    wxDECLARE_NO_COPY_CLASS(wxTimerImpl);
};

#endif // _WX_TIMERIMPL_H_BASE_
