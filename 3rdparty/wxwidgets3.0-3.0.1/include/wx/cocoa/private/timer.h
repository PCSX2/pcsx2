/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/private/timer.h
// Purpose:     Cocoa wxTimer class
// Author:      Ryan Norton
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_PRIVATE_TIMER_H_
#define _WX_COCOA_PRIVATE_TIMER_H_

#include "wx/private/timer.h"

#include "wx/cocoa/ObjcRef.h"

//-----------------------------------------------------------------------------
// wxTimer
//-----------------------------------------------------------------------------

DECLARE_WXCOCOA_OBJC_CLASS(NSTimer);

class WXDLLIMPEXP_CORE wxCocoaTimerImpl : public wxTimerImpl
{
public:
    wxCocoaTimerImpl(wxTimer* timer) : wxTimerImpl(timer) { Init(); }
    virtual ~wxCocoaTimerImpl();

    virtual bool Start(int millisecs = -1, bool oneShot = false);
    virtual void Stop();

    virtual bool IsRunning() const;

    WX_NSTimer GetNSTimer() { return m_cocoaNSTimer; }

protected:
    void Init();

private:
    WX_NSTimer m_cocoaNSTimer;
};

#endif // _WX_COCOA_PRIVATE_TIMER_H_
