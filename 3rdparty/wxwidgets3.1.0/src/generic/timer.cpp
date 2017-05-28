/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/timer.cpp
// Purpose:     wxTimer implementation
// Author:      Vaclav Slavik
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// ----------------------------------------------------------------------------
// NB: when using generic wxTimer implementation in your port, you *must* call
//     wxTimer::NotifyTimers() often enough. The ideal place for this
//     is in wxEventLoop::Dispatch().
// ----------------------------------------------------------------------------

#if wxUSE_TIMER

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/module.h"
#endif

#include "wx/apptrait.h"
#include "wx/generic/private/timer.h"

// ----------------------------------------------------------------------------
// Time input function
// ----------------------------------------------------------------------------

#define GetMillisecondsTime wxGetLocalTimeMillis

typedef wxLongLong wxTimerTick_t;

#if wxUSE_LONGLONG_WX
    #define wxTimerTickFmtSpec wxLongLongFmtSpec "d"
    #define wxTimerTickPrintfArg(tt) (tt.GetValue())
#else // using native wxLongLong
    #define wxTimerTickFmtSpec wxT("s")
    #define wxTimerTickPrintfArg(tt) (tt.ToString().c_str())
#endif // wx/native long long

inline bool wxTickGreaterEqual(wxTimerTick_t x, wxTimerTick_t y)
{
    return x >= y;
}

// ----------------------------------------------------------------------------
// helper structures and wxTimerScheduler
// ----------------------------------------------------------------------------

class wxTimerDesc
{
public:
    wxTimerDesc(wxGenericTimerImpl *t) :
        timer(t), running(false), next(NULL), prev(NULL),
        shotTime(0), deleteFlag(NULL) {}

    wxGenericTimerImpl  *timer;
    bool                running;
    wxTimerDesc         *next, *prev;
    wxTimerTick_t        shotTime;
    volatile bool       *deleteFlag; // see comment in ~wxTimer
};

class wxTimerScheduler
{
public:
    wxTimerScheduler() : m_timers(NULL) {}

    void QueueTimer(wxTimerDesc *desc, wxTimerTick_t when = 0);
    void RemoveTimer(wxTimerDesc *desc);
    void NotifyTimers();

private:
    wxTimerDesc *m_timers;
};

void wxTimerScheduler::QueueTimer(wxTimerDesc *desc, wxTimerTick_t when)
{
    if ( desc->running )
        return; // already scheduled

    if ( when == 0 )
        when = GetMillisecondsTime() + desc->timer->GetInterval();
    desc->shotTime = when;
    desc->running = true;

    wxLogTrace( wxT("timer"),
                wxT("queued timer %p at tick %") wxTimerTickFmtSpec,
               desc->timer,  wxTimerTickPrintfArg(when));

    if ( m_timers )
    {
        wxTimerDesc *d = m_timers;
        while ( d->next && d->next->shotTime < when ) d = d->next;
        desc->next = d->next;
        desc->prev = d;
        if ( d->next )
            d->next->prev = desc;
        d->next = desc;
    }
    else
    {
        m_timers = desc;
        desc->prev = desc->next = NULL;
    }
}

void wxTimerScheduler::RemoveTimer(wxTimerDesc *desc)
{
    desc->running = false;
    if ( desc == m_timers )
        m_timers = desc->next;
    if ( desc->prev )
        desc->prev->next = desc->next;
    if ( desc->next )
        desc->next->prev = desc->prev;
    desc->prev = desc->next = NULL;
}

void wxTimerScheduler::NotifyTimers()
{
    if ( m_timers )
    {
        bool oneShot;
        volatile bool timerDeleted;
        wxTimerTick_t now = GetMillisecondsTime();

        for ( wxTimerDesc *desc = m_timers; desc; desc = desc->next )
        {
            if ( desc->running && wxTickGreaterEqual(now, desc->shotTime) )
            {
                oneShot = desc->timer->IsOneShot();
                RemoveTimer(desc);

                timerDeleted = false;
                desc->deleteFlag = &timerDeleted;
                desc->timer->Notify();

                if ( !timerDeleted )
                {
                    wxLogTrace( wxT("timer"),
                                wxT("notified timer %p sheduled for %")
                                wxTimerTickFmtSpec,
                                desc->timer,
                                wxTimerTickPrintfArg(desc->shotTime) );

                    desc->deleteFlag = NULL;
                    if ( !oneShot )
                        QueueTimer(desc, now + desc->timer->GetInterval());
                }
                else
                {
                    desc = m_timers;
                    if (!desc)
                        break;
                }
            }
        }
    }
}


// ----------------------------------------------------------------------------
// wxTimer
// ----------------------------------------------------------------------------

wxTimerScheduler *gs_scheduler = NULL;

void wxGenericTimerImpl::Init()
{
    if ( !gs_scheduler )
        gs_scheduler = new wxTimerScheduler;
    m_desc = new wxTimerDesc(this);
}

wxGenericTimerImpl::~wxGenericTimerImpl()
{
    wxLogTrace( wxT("timer"), wxT("destroying timer %p..."), this);
    if ( IsRunning() )
        Stop();

    // NB: this is a hack: wxTimerScheduler must have some way of knowing
    //     that wxTimer object was deleted under its hands -- this may
    //     happen if somebody is really nasty and deletes the timer
    //     from wxTimer::Notify()
    if ( m_desc->deleteFlag != NULL )
        *m_desc->deleteFlag = true;

    delete m_desc;
    wxLogTrace( wxT("timer"), wxT("    ...done destroying timer %p..."), this);
}

bool wxGenericTimerImpl::IsRunning() const
{
    return m_desc->running;
}

bool wxGenericTimerImpl::Start(int millisecs, bool oneShot)
{
    wxLogTrace( wxT("timer"), wxT("started timer %p: %i ms, oneshot=%i"),
               this, millisecs, oneShot);

     if ( !wxTimerImpl::Start(millisecs, oneShot) )
         return false;

    gs_scheduler->QueueTimer(m_desc);
    return true;
}

void wxGenericTimerImpl::Stop()
{
    if ( !m_desc->running ) return;

    gs_scheduler->RemoveTimer(m_desc);
}

/*static*/ void wxGenericTimerImpl::NotifyTimers()
{
    if ( gs_scheduler )
        gs_scheduler->NotifyTimers();
}



// A module to deallocate memory properly:
class wxTimerModule: public wxModule
{
    wxDECLARE_DYNAMIC_CLASS(wxTimerModule);
public:
    wxTimerModule() {}
    bool OnInit() { return true; }
    void OnExit() { wxDELETE(gs_scheduler); }
};

wxIMPLEMENT_DYNAMIC_CLASS(wxTimerModule, wxModule);

// ----------------------------------------------------------------------------
// wxGUIAppTraits
// ----------------------------------------------------------------------------

wxTimerImpl *wxGUIAppTraits::CreateTimerImpl(wxTimer *timer)
{
    return new wxGenericTimerImpl(timer);
}


#endif //wxUSE_TIMER
