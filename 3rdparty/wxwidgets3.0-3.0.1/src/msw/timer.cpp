/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/timer.cpp
// Purpose:     wxTimer implementation
// Author:      Julian Smart
// Modified by: Vadim Zeitlin (use hash map instead of list, global rewrite)
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TIMER

#include "wx/msw/private/timer.h"

#ifndef WX_PRECOMP
    #include "wx/list.h"
    #include "wx/event.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/hashmap.h"
    #include "wx/module.h"
#endif

#include "wx/msw/private.h"
#include "wx/msw/private/hiddenwin.h"

// ----------------------------------------------------------------------------
// private globals
// ----------------------------------------------------------------------------

// define a hash containing all the timers: it is indexed by timer id and
// contains the corresponding timer
WX_DECLARE_HASH_MAP(WPARAM, wxMSWTimerImpl *, wxIntegerHash, wxIntegerEqual,
                    wxTimerMap);

// instead of using a global here, wrap it in a static function as otherwise it
// could have been used before being initialized if a timer object were created
// globally
static wxTimerMap& TimerMap()
{
    static wxTimerMap s_timerMap;

    return s_timerMap;
}

// This gets a unique, non-zero timer ID and creates an entry in the TimerMap
UINT_PTR GetNewTimerId(wxMSWTimerImpl *t)
{
    static UINT_PTR lastTimerId = 0;

    while (lastTimerId == 0 ||
            TimerMap().find(lastTimerId) != TimerMap().end())
    {
        lastTimerId = lastTimerId + 1;
    }

    TimerMap()[lastTimerId] = t;

    return lastTimerId;
}



// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

LRESULT APIENTRY _EXPORT wxTimerWndProc(HWND hWnd, UINT message,
                                        WPARAM wParam, LPARAM lParam);

// ----------------------------------------------------------------------------
// wxTimerHiddenWindowModule: used to manage the hidden window used for
// catching timer messages (we need a module to ensure that the window is
// always deleted)
// ----------------------------------------------------------------------------

class wxTimerHiddenWindowModule : public wxModule
{
public:
    // module init/finalize
    virtual bool OnInit();
    virtual void OnExit();

    // get the hidden window (creates on demand)
    static HWND GetHWND();

private:
    // the HWND of the hidden window
    static HWND ms_hwnd;

    // the class used to create it
    static const wxChar *ms_className;

    DECLARE_DYNAMIC_CLASS(wxTimerHiddenWindowModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxTimerHiddenWindowModule, wxModule)

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// wxMSWTimerImpl class
// ----------------------------------------------------------------------------

bool wxMSWTimerImpl::Start(int milliseconds, bool oneShot)
{
    if ( !wxTimerImpl::Start(milliseconds, oneShot) )
        return false;

    m_id = GetNewTimerId(this);
    // SetTimer() normally returns just idTimer but this might change in the
    // future so use its return value to be safe
    UINT_PTR ret = ::SetTimer
             (
              wxTimerHiddenWindowModule::GetHWND(),  // window for WM_TIMER
              m_id,                                  // timer ID to create
              (UINT)m_milli,                         // delay
              NULL                                   // timer proc (unused)
             );

    if ( ret == 0 )
    {
        wxLogSysError(_("Couldn't create a timer"));

        return false;
    }

    return true;
}

void wxMSWTimerImpl::Stop()
{
    ::KillTimer(wxTimerHiddenWindowModule::GetHWND(), m_id);
    TimerMap().erase(m_id);
    m_id = 0;
}

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

void wxProcessTimer(wxMSWTimerImpl& timer)
{
    wxASSERT_MSG( timer.IsRunning(), wxT("bogus timer id") );

    if ( timer.IsOneShot() )
        timer.Stop();

    timer.Notify();
}


LRESULT APIENTRY _EXPORT wxTimerWndProc(HWND hWnd, UINT message,
                                        WPARAM wParam, LPARAM lParam)
{
    if ( message == WM_TIMER )
    {
        wxTimerMap::iterator node = TimerMap().find(wParam);

        if ( node != TimerMap().end() )
        {
            wxProcessTimer(*(node->second));

            return 0;
        }
        //else: Unknown timer, probably one of our timers that had fired just
        //      before being removed from the timers map by Stop().
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

// ----------------------------------------------------------------------------
// wxTimerHiddenWindowModule functions
// ----------------------------------------------------------------------------


HWND wxTimerHiddenWindowModule::ms_hwnd = NULL;

const wxChar *wxTimerHiddenWindowModule::ms_className = NULL;

bool wxTimerHiddenWindowModule::OnInit()
{
    // do not initialize ms_hwnd to ms_className to NULL here: it may happen
    // that our GetHWND() is called before the modules are initialized if a
    // timer is created from wxApp-derived class ctor and in this case we
    // shouldn't overwrite it

    return true;
}

void wxTimerHiddenWindowModule::OnExit()
{
    if ( ms_hwnd )
    {
        if ( !::DestroyWindow(ms_hwnd) )
        {
            wxLogLastError(wxT("DestroyWindow(wxTimerHiddenWindow)"));
        }

        ms_hwnd = NULL;
    }

    if ( ms_className )
    {
        if ( !::UnregisterClass(ms_className, wxGetInstance()) )
        {
            wxLogLastError(wxT("UnregisterClass(\"wxTimerHiddenWindow\")"));
        }

        ms_className = NULL;
    }
}

/* static */
HWND wxTimerHiddenWindowModule::GetHWND()
{
    static const wxChar *HIDDEN_WINDOW_CLASS = wxT("wxTimerHiddenWindow");
    if ( !ms_hwnd )
    {
        ms_hwnd = wxCreateHiddenWindow(&ms_className, HIDDEN_WINDOW_CLASS,
                                     wxTimerWndProc);
    }

    return ms_hwnd;
}

#endif // wxUSE_TIMER
