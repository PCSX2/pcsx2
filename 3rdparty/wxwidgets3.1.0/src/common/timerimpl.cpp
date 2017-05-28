/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/timerimpl.cpp
// Purpose:     wxTimerBase implementation
// Author:      Julian Smart, Guillermo Rodriguez, Vadim Zeitlin
// Modified by: VZ: extracted all non-wxTimer stuff in stopwatch.cpp (20.06.03)
// Created:     04/01/98
// Copyright:   (c) Julian Smart
//              (c) 1999 Guillermo Rodriguez <guille@iies.es>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// wxWin headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TIMER

#include "wx/private/timer.h"
#include "wx/utils.h"               // for wxNewId()
#include "wx/thread.h"

wxTimerImpl::wxTimerImpl(wxTimer *timer)
{
    m_timer = timer;
    m_owner = NULL;
    m_idTimer = wxID_ANY;
    m_milli = 0;
    m_oneShot = false;
}

void wxTimerImpl::SetOwner(wxEvtHandler *owner, int timerid)
{
    m_owner = owner;
    m_idTimer = timerid == wxID_ANY ? wxNewId() : timerid;
}

void wxTimerImpl::SendEvent()
{
    wxTimerEvent event(*m_timer);
    (void)m_owner->SafelyProcessEvent(event);
}

bool wxTimerImpl::Start(int milliseconds, bool oneShot)
{
    // under MSW timers only work when they're started from the main thread so
    // let the caller know about it
#if wxUSE_THREADS
    wxASSERT_MSG( wxThread::IsMain(),
                  wxT("timer can only be started from the main thread") );
#endif // wxUSE_THREADS

    if ( IsRunning() )
    {
        // not stopping the already running timer might work for some
        // platforms (no problems under MSW) but leads to mysterious crashes
        // on the others (GTK), so to be on the safe side do it here
        Stop();
    }

    if ( milliseconds != -1 )
    {
        m_milli = milliseconds;
    }

    m_oneShot = oneShot;

    return true;
}


#endif // wxUSE_TIMER

