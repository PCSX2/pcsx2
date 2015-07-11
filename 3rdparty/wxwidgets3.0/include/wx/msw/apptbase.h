///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/apptbase.h
// Purpose:     declaration of wxAppTraits for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.06.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_APPTBASE_H_
#define _WX_MSW_APPTBASE_H_

// ----------------------------------------------------------------------------
// wxAppTraits: the MSW version adds extra hooks needed by MSW-only code
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxAppTraits : public wxAppTraitsBase
{
public:
    // wxExecute() support methods
    // ---------------------------

    // called before starting to wait for the child termination, may return
    // some opaque data which will be passed later to AfterChildWaitLoop()
    virtual void *BeforeChildWaitLoop() = 0;

    // called after starting to wait for the child termination, the parameter
    // is the return value of BeforeChildWaitLoop()
    virtual void AfterChildWaitLoop(void *data) = 0;


#if wxUSE_THREADS
    // wxThread helpers
    // ----------------

    // process a message while waiting for a(nother) thread, should return
    // false if and only if we have to exit the application
    virtual bool DoMessageFromThreadWait() = 0;

    // wait for the handle to be signaled, return WAIT_OBJECT_0 if it is or, in
    // the GUI code, WAIT_OBJECT_0 + 1 if a Windows message arrived
    virtual WXDWORD WaitForThread(WXHANDLE hThread, int flags) = 0;
#endif // wxUSE_THREADS


#ifndef __WXWINCE__
    // console helpers
    // ---------------

    // this method can be overridden by a derived class to always return true
    // or false to force [not] using the console for output to stderr
    //
    // by default console applications always return true from here while the
    // GUI ones only return true if they're being run from console and there is
    // no other activity happening in this console
    virtual bool CanUseStderr() = 0;

    // write text to the console, return true if ok or false on error
    virtual bool WriteToStderr(const wxString& text) = 0;
#endif // !__WXWINCE__

protected:
#if wxUSE_THREADS
    // implementation of WaitForThread() for the console applications which is
    // also used by the GUI code if it doesn't [yet|already] dispatch events
    WXDWORD DoSimpleWaitForThread(WXHANDLE hThread);
#endif // wxUSE_THREADS
};

#endif // _WX_MSW_APPTBASE_H_

