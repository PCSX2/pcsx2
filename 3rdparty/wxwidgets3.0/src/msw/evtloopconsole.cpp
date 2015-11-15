///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/evtloopconsole.cpp
// Purpose:     wxConsoleEventLoop class for Windows
// Author:      Vadim Zeitlin
// Modified by:
// Created:     01.06.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/msw/wrapwin.h"
#endif //WX_PRECOMP

#include "wx/evtloop.h"

// ============================================================================
// wxMSWEventLoopBase implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctor/dtor
// ----------------------------------------------------------------------------

wxMSWEventLoopBase::wxMSWEventLoopBase()
{
    m_shouldExit = false;
    m_exitcode = 0;
}

// ----------------------------------------------------------------------------
// wxEventLoop message processing dispatching
// ----------------------------------------------------------------------------

bool wxMSWEventLoopBase::Pending() const
{
    MSG msg;
    return ::PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE) != 0;
}

bool wxMSWEventLoopBase::GetNextMessage(WXMSG* msg)
{
    const BOOL rc = ::GetMessage(msg, NULL, 0, 0);

    if ( rc == 0 )
    {
        // got WM_QUIT
        return false;
    }

    if ( rc == -1 )
    {
        // should never happen, but let's test for it nevertheless
        wxLogLastError(wxT("GetMessage"));

        // still break from the loop
        return false;
    }

    return true;
}

int wxMSWEventLoopBase::GetNextMessageTimeout(WXMSG *msg, unsigned long timeout)
{
    // MsgWaitForMultipleObjects() won't notice any input which was already
    // examined (e.g. using PeekMessage()) but not yet removed from the queue
    // so we need to remove any immediately messages manually
    //
    // NB: using MsgWaitForMultipleObjectsEx() could simplify the code here but
    //     it is not available in very old Windows versions
    if ( !::PeekMessage(msg, 0, 0, 0, PM_REMOVE) )
    {
        // we use this function just in order to not block longer than the
        // given timeout, so we don't pass any handles to it at all
        DWORD rc = ::MsgWaitForMultipleObjects
                     (
                        0, NULL,
                        FALSE,
                        timeout,
                        QS_ALLINPUT
                     );

        switch ( rc )
        {
            default:
                wxLogDebug("unexpected MsgWaitForMultipleObjects() return "
                           "value %lu", rc);
                // fall through

            case WAIT_TIMEOUT:
                return -1;

            case WAIT_OBJECT_0:
                if ( !::PeekMessage(msg, 0, 0, 0, PM_REMOVE) )
                {
                    // somehow it may happen that MsgWaitForMultipleObjects()
                    // returns true but there are no messages -- just treat it
                    // the same as timeout then
                    return -1;
                }
                break;
        }
    }

    return msg->message != WM_QUIT;
}

// ============================================================================
// wxConsoleEventLoop implementation
// ============================================================================

#if wxUSE_CONSOLE_EVENTLOOP

void wxConsoleEventLoop::WakeUp()
{
#if wxUSE_THREADS
    wxWakeUpMainThread();
#endif
}

void wxConsoleEventLoop::ProcessMessage(WXMSG *msg)
{
    ::DispatchMessage(msg);
}

bool wxConsoleEventLoop::Dispatch()
{
    MSG msg;
    if ( !GetNextMessage(&msg) )
        return false;

    ProcessMessage(&msg);

    return !m_shouldExit;
}

int wxConsoleEventLoop::DispatchTimeout(unsigned long timeout)
{
    MSG msg;
    int rc = GetNextMessageTimeout(&msg, timeout);
    if ( rc != 1 )
        return rc;

    ProcessMessage(&msg);

    return !m_shouldExit;
}

#endif // wxUSE_CONSOLE_EVENTLOOP
