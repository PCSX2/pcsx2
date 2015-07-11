///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/evtloop.h
// Purpose:     wxEventLoop class for wxMSW port
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-07-31
// Copyright:   (c) 2003-2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_EVTLOOP_H_
#define _WX_MSW_EVTLOOP_H_

#include "wx/dynarray.h"
#include "wx/msw/wrapwin.h"
#include "wx/window.h"
#include "wx/msw/evtloopconsole.h" // for wxMSWEventLoopBase

// ----------------------------------------------------------------------------
// wxEventLoop
// ----------------------------------------------------------------------------

WX_DECLARE_EXPORTED_OBJARRAY(MSG, wxMSGArray);

class WXDLLIMPEXP_CORE wxGUIEventLoop : public wxMSWEventLoopBase
{
public:
    wxGUIEventLoop() { }

    // process a single message: calls PreProcessMessage() before dispatching
    // it
    virtual void ProcessMessage(WXMSG *msg);

    // preprocess a message, return true if processed (i.e. no further
    // dispatching required)
    virtual bool PreProcessMessage(WXMSG *msg);

    // set the critical window: this is the window such that all the events
    // except those to this window (and its children) stop to be processed
    // (typical examples: assert or crash report dialog)
    //
    // calling this function with NULL argument restores the normal event
    // handling
    static void SetCriticalWindow(wxWindowMSW *win) { ms_winCritical = win; }

    // return true if there is no critical window or if this window is [a child
    // of] the critical one
    static bool AllowProcessing(wxWindowMSW *win)
    {
        return !ms_winCritical || IsChildOfCriticalWindow(win);
    }

    // override/implement base class virtuals
    virtual bool Dispatch();
    virtual int DispatchTimeout(unsigned long timeout);
    virtual void WakeUp();
    virtual bool YieldFor(long eventsToProcess);

protected:
    virtual void OnNextIteration();

private:
    // check if the given window is a child of ms_winCritical (which must be
    // non NULL)
    static bool IsChildOfCriticalWindow(wxWindowMSW *win);

    // array of messages used for temporary storage by YieldFor()
    wxMSGArray m_arrMSG;

    // critical window or NULL
    static wxWindowMSW *ms_winCritical;
};

#endif // _WX_MSW_EVTLOOP_H_
