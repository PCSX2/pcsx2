/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/timercmn.cpp
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

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

#include "wx/timer.h"
#include "wx/apptrait.h"
#include "wx/private/timer.h"

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxTimerEvent, wxEvent);

wxDEFINE_EVENT(wxEVT_TIMER, wxTimerEvent);

// ============================================================================
// wxTimerBase implementation
// ============================================================================

wxTimer::~wxTimer()
{
    Stop();

    delete m_impl;
}

void wxTimer::Init()
{
    wxAppTraits * const traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
    m_impl = traits ? traits->CreateTimerImpl(this) : NULL;
    if ( !m_impl )
    {
        wxFAIL_MSG( wxT("No timer implementation for this platform") );

    }
}

// ============================================================================
// rest of wxTimer implementation forwarded to wxTimerImpl
// ============================================================================

void wxTimer::SetOwner(wxEvtHandler *owner, int timerid)
{
    wxCHECK_RET( m_impl, wxT("uninitialized timer") );

    m_impl->SetOwner(owner, timerid);
}

wxEvtHandler *wxTimer::GetOwner() const
{
    wxCHECK_MSG( m_impl, NULL, wxT("uninitialized timer") );

    return m_impl->GetOwner();
}

bool wxTimer::Start(int milliseconds, bool oneShot)
{
    wxCHECK_MSG( m_impl, false, wxT("uninitialized timer") );

    return m_impl->Start(milliseconds, oneShot);
}

void wxTimer::Stop()
{
    wxCHECK_RET( m_impl, wxT("uninitialized timer") );

    if ( m_impl->IsRunning() )
        m_impl->Stop();
}

void wxTimer::Notify()
{
    // the base class version generates an event if it has owner - which it
    // should because otherwise nobody can process timer events
    wxCHECK_RET( GetOwner(), wxT("wxTimer::Notify() should be overridden.") );

    m_impl->SendEvent();
}

bool wxTimer::IsRunning() const
{
    wxCHECK_MSG( m_impl, false, wxT("uninitialized timer") );

    return m_impl->IsRunning();
}

int wxTimer::GetId() const
{
    wxCHECK_MSG( m_impl, wxID_ANY, wxT("uninitialized timer") );

    return m_impl->GetId();
}

int wxTimer::GetInterval() const
{
    wxCHECK_MSG( m_impl, -1, wxT("uninitialized timer") );

    return m_impl->GetInterval();
}

bool wxTimer::IsOneShot() const
{
    wxCHECK_MSG( m_impl, false, wxT("uninitialized timer") );

    return m_impl->IsOneShot();
}

#endif // wxUSE_TIMER

