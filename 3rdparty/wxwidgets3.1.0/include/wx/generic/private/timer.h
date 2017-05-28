/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/private/timer.h
// Purpose:     Generic implementation of wxTimer class
// Author:      Vaclav Slavik
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_PRIVATE_TIMER_H_
#define _WX_GENERIC_PRIVATE_TIMER_H_

#if wxUSE_TIMER

#include "wx/private/timer.h"

//-----------------------------------------------------------------------------
// wxTimer
//-----------------------------------------------------------------------------

class wxTimerDesc;

class WXDLLIMPEXP_BASE wxGenericTimerImpl : public wxTimerImpl
{
public:
    wxGenericTimerImpl(wxTimer* timer) : wxTimerImpl(timer) { Init(); }
    virtual ~wxGenericTimerImpl();

    virtual bool Start(int millisecs = -1, bool oneShot = false);
    virtual void Stop();

    virtual bool IsRunning() const;

    // implementation
    static void NotifyTimers();

protected:
    void Init();

private:
    wxTimerDesc *m_desc;
};

#endif // wxUSE_TIMER

#endif // _WX_GENERIC_PRIVATE_TIMER_H_
