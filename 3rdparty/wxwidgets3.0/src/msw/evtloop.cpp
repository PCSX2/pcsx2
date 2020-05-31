///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/evtloop.cpp
// Purpose:     implements wxEventLoop for wxMSW port
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

#include "wx/evtloop.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/app.h"
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/thread.h"
#include "wx/except.h"
#include "wx/msw/private.h"
#include "wx/scopeguard.h"

#include "wx/tooltip.h"
#if wxUSE_THREADS
    // define the list of MSG strutures
    WX_DECLARE_LIST(MSG, wxMsgList);

    #include "wx/listimpl.cpp"

    WX_DEFINE_LIST(wxMsgList)
#endif // wxUSE_THREADS

// ============================================================================
// GUI wxEventLoop implementation
// ============================================================================

wxWindowMSW *wxGUIEventLoop::ms_winCritical = NULL;

bool wxGUIEventLoop::IsChildOfCriticalWindow(wxWindowMSW *win)
{
    while ( win )
    {
        if ( win == ms_winCritical )
            return true;

        win = win->GetParent();
    }

    return false;
}

bool wxGUIEventLoop::PreProcessMessage(WXMSG *msg)
{
    HWND hwnd = msg->hwnd;
    wxWindow *wndThis = wxGetWindowFromHWND((WXHWND)hwnd);
    wxWindow *wnd;

    // this might happen if we're in a modeless dialog, or if a wx control has
    // children which themselves were not created by wx (i.e. wxActiveX control children)
    if ( !wndThis )
    {
        while ( hwnd && (::GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD ))
        {
            hwnd = ::GetParent(hwnd);

            // If the control has a wx parent, break and give the parent a chance
            // to process the window message
            wndThis = wxGetWindowFromHWND((WXHWND)hwnd);
            if (wndThis != NULL)
                break;
        }

        if ( !wndThis )
        {
            // this may happen if the event occurred in a standard modeless dialog (the
            // only example of which I know of is the find/replace dialog) - then call
            // IsDialogMessage() to make TAB navigation in it work

            // NOTE: IsDialogMessage() just eats all the messages (i.e. returns true for
            // them) if we call it for the control itself
            return hwnd && ::IsDialogMessage(hwnd, msg) != 0;
        }
    }

    if ( !AllowProcessing(wndThis) )
    {
        // not a child of critical window, so we eat the event but take care to
        // stop an endless stream of WM_PAINTs which would have resulted if we
        // didn't validate the invalidated part of the window
        if ( msg->message == WM_PAINT )
            ::ValidateRect(hwnd, NULL);

        return true;
    }

#if wxUSE_TOOLTIPS
    // we must relay WM_MOUSEMOVE events to the tooltip ctrl if we want it to
    // popup the tooltip bubbles
    if ( msg->message == WM_MOUSEMOVE )
    {
        // we should do it if one of window children has an associated tooltip
        // (and not just if the window has a tooltip itself)
        if ( wndThis->HasToolTips() )
            wxToolTip::RelayEvent((WXMSG *)msg);
    }
#endif // wxUSE_TOOLTIPS

    // allow the window to prevent certain messages from being
    // translated/processed (this is currently used by wxTextCtrl to always
    // grab Ctrl-C/V/X, even if they are also accelerators in some parent)
    if ( !wndThis->MSWShouldPreProcessMessage((WXMSG *)msg) )
    {
        return false;
    }

    // try translations first: the accelerators override everything
    for ( wnd = wndThis; wnd; wnd = wnd->GetParent() )
    {
        if ( wnd->MSWTranslateMessage((WXMSG *)msg))
            return true;

        // stop at first top level window, i.e. don't try to process the key
        // strokes originating in a dialog using the accelerators of the parent
        // frame - this doesn't make much sense
        if ( wnd->IsTopLevel() )
            break;
    }

    // now try the other hooks (kbd navigation is handled here)
    for ( wnd = wndThis; wnd; wnd = wnd->GetParent() )
    {
        if ( wnd->MSWProcessMessage((WXMSG *)msg) )
            return true;

        // also stop at first top level window here, just as above because
        // if we don't do this, pressing ESC on a modal dialog shown as child
        // of a modal dialog with wxID_CANCEL will cause the parent dialog to
        // be closed, for example
        if ( wnd->IsTopLevel() )
            break;
    }

    // no special preprocessing for this message, dispatch it normally
    return false;
}

void wxGUIEventLoop::ProcessMessage(WXMSG *msg)
{
    // give us the chance to preprocess the message first
    if ( !PreProcessMessage(msg) )
    {
        // if it wasn't done, dispatch it to the corresponding window
        ::TranslateMessage(msg);
        ::DispatchMessage(msg);
    }
}

bool wxGUIEventLoop::Dispatch()
{
    MSG msg;
    if ( !GetNextMessage(&msg) )
        return false;

#if wxUSE_THREADS
    wxASSERT_MSG( wxThread::IsMain(),
                  wxT("only the main thread can process Windows messages") );

    static bool s_hadGuiLock = true;
    static wxMsgList s_aSavedMessages;

    // if a secondary thread owning the mutex is doing GUI calls, save all
    // messages for later processing - we can't process them right now because
    // it will lead to recursive library calls (and we're not reentrant)
    if ( !wxGuiOwnedByMainThread() )
    {
        s_hadGuiLock = false;

        // leave out WM_COMMAND messages: too dangerous, sometimes
        // the message will be processed twice
        if ( !wxIsWaitingForThread() || msg.message != WM_COMMAND )
        {
            MSG* pMsg = new MSG(msg);
            s_aSavedMessages.Append(pMsg);
        }

        return true;
    }
    else
    {
        // have we just regained the GUI lock? if so, post all of the saved
        // messages
        //
        // FIXME of course, it's not _exactly_ the same as processing the
        //       messages normally - expect some things to break...
        if ( !s_hadGuiLock )
        {
            s_hadGuiLock = true;

            wxMsgList::compatibility_iterator node = s_aSavedMessages.GetFirst();
            while (node)
            {
                MSG* pMsg = node->GetData();
                s_aSavedMessages.Erase(node);

                ProcessMessage(pMsg);
                delete pMsg;

                node = s_aSavedMessages.GetFirst();
            }
        }
    }
#endif // wxUSE_THREADS

    ProcessMessage(&msg);

    return true;
}

int wxGUIEventLoop::DispatchTimeout(unsigned long timeout)
{
    MSG msg;
    int rc = GetNextMessageTimeout(&msg, timeout);
    if ( rc != 1 )
        return rc;

    ProcessMessage(&msg);

    return 1;
}

void wxGUIEventLoop::OnNextIteration()
{
#if wxUSE_THREADS
    wxMutexGuiLeaveOrEnter();
#endif // wxUSE_THREADS
}

void wxGUIEventLoop::WakeUp()
{
    ::PostMessage(NULL, WM_NULL, 0, 0);
}


// ----------------------------------------------------------------------------
// Yield to incoming messages
// ----------------------------------------------------------------------------

#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(wxMSGArray);

bool wxGUIEventLoop::YieldFor(long eventsToProcess)
{
    // set the flag and don't forget to reset it before returning
    m_isInsideYield = true;
    m_eventsToProcessInsideYield = eventsToProcess;

    wxON_BLOCK_EXIT_SET(m_isInsideYield, false);

#if wxUSE_LOG
    // disable log flushing from here because a call to wxYield() shouldn't
    // normally result in message boxes popping up &c
    wxLog::Suspend();

    // ensure the logs will be flashed again when we exit
    wxON_BLOCK_EXIT0(wxLog::Resume);
#endif // wxUSE_LOG

    // we don't want to process WM_QUIT from here - it should be processed in
    // the main event loop in order to stop it
    MSG msg;
    int nPaintsReceived = 0;
    while ( PeekMessage(&msg, (HWND)0, 0, 0, PM_NOREMOVE) &&
            msg.message != WM_QUIT )
    {
#if wxUSE_THREADS
        wxMutexGuiLeaveOrEnter();
#endif // wxUSE_THREADS

        if (msg.message == WM_PAINT)
        {
            // NOTE: WM_PAINTs are categorized as wxEVT_CATEGORY_UI
            if ((eventsToProcess & wxEVT_CATEGORY_UI) == 0)
            {
                // this msg is not going to be dispatched...
                // however WM_PAINT is special: until there are damaged
                // windows, Windows will keep sending it forever!
                if (nPaintsReceived > 10)
                {
                    // we got 10 WM_PAINT consecutive messages...
                    // we must have reached the tail of the message queue:
                    // we're now getting _only_ WM_PAINT events and this will
                    // continue forever (since we don't dispatch them
                    // because of the user-specified eventsToProcess mask)...
                    // break out of this loop!
                    break;
                }
                else
                    nPaintsReceived++;
            }
            //else: we're going to dispatch it below,
            //      so we don't need to take any special action
        }
        else
        {
            // reset the counter of consecutive WM_PAINT messages received:
            nPaintsReceived = 0;
        }

        // choose a wxEventCategory for this Windows message
        bool processNow;
        switch (msg.message)
        {
#if !defined(__WXWINCE__)
            case WM_NCMOUSEMOVE:

            case WM_NCLBUTTONDOWN:
            case WM_NCLBUTTONUP:
            case WM_NCLBUTTONDBLCLK:
            case WM_NCRBUTTONDOWN:
            case WM_NCRBUTTONUP:
            case WM_NCRBUTTONDBLCLK:
            case WM_NCMBUTTONDOWN:
            case WM_NCMBUTTONUP:
            case WM_NCMBUTTONDBLCLK:
#endif

            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_DEADCHAR:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_SYSCHAR:
            case WM_SYSDEADCHAR:
#ifdef WM_UNICHAR
            case WM_UNICHAR:
#endif
            case WM_HOTKEY:
            case WM_IME_STARTCOMPOSITION:
            case WM_IME_ENDCOMPOSITION:
            case WM_IME_COMPOSITION:
            case WM_COMMAND:
            case WM_SYSCOMMAND:

            case WM_IME_SETCONTEXT:
            case WM_IME_NOTIFY:
            case WM_IME_CONTROL:
            case WM_IME_COMPOSITIONFULL:
            case WM_IME_SELECT:
            case WM_IME_CHAR:
            case WM_IME_KEYDOWN:
            case WM_IME_KEYUP:

#if !defined(__WXWINCE__)
            case WM_MOUSEHOVER:
            case WM_MOUSELEAVE:
#endif
#ifdef WM_NCMOUSELEAVE
            case WM_NCMOUSELEAVE:
#endif

            case WM_CUT:
            case WM_COPY:
            case WM_PASTE:
            case WM_CLEAR:
            case WM_UNDO:

            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
                processNow = (eventsToProcess & wxEVT_CATEGORY_USER_INPUT) != 0;
                break;

            case WM_TIMER:
                processNow = (eventsToProcess & wxEVT_CATEGORY_TIMER) != 0;
                break;

            default:
                if (msg.message < WM_USER)
                {
                    // 0;WM_USER-1 is the range of message IDs reserved for use
                    // by the system.
                    // there are too many of these types of messages to handle
                    // them in this switch
                    processNow = (eventsToProcess & wxEVT_CATEGORY_UI) != 0;
                }
                else
                {
                    // Process all the unknown messages. We must do it because
                    // failure to process some of them can be fatal, e.g. if we
                    // don't dispatch WM_APP+2 then embedded IE ActiveX
                    // controls don't work any more, see #14027. And there may
                    // be more examples like this, so dispatch all unknown
                    // messages immediately to be safe.
                    processNow = true;
                }
        }

        // should we process this event now?
        if ( processNow )
        {
            if ( !wxTheApp->Dispatch() )
                break;
        }
        else
        {
            // remove the message and store it
            ::GetMessage(&msg, NULL, 0, 0);
            m_arrMSG.Add(msg);
        }
    }

    // if there are pending events, we must process them.
    if (wxTheApp)
        wxTheApp->ProcessPendingEvents();

    // put back unprocessed events in the queue
    DWORD id = GetCurrentThreadId();
    for (size_t i=0; i<m_arrMSG.GetCount(); i++)
    {
        PostThreadMessage(id, m_arrMSG[i].message,
                          m_arrMSG[i].wParam, m_arrMSG[i].lParam);
    }

    m_arrMSG.Clear();

    return true;
}
