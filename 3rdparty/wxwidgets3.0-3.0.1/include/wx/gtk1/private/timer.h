/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/private/timer.h
// Purpose:     wxTimerImpl for wxGTK
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK1_PRIVATE_TIMER_H_
#define _WX_GTK1_PRIVATE_TIMER_H_

#include "wx/private/timer.h"

//-----------------------------------------------------------------------------
// wxTimer
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGTKTimerImpl : public wxTimerImpl
{
public:
    wxGTKTimerImpl(wxTimer *timer) : wxTimerImpl(timer) { m_tag = -1; }

    virtual bool Start(int millisecs = -1, bool oneShot = FALSE);
    virtual void Stop();

    virtual bool IsRunning() const { return m_tag != -1; }

private:
    // registered timeout id, -1 if the timer isn't running
    int m_tag;
};

#endif // _WX_GTK1_PRIVATE_TIMER_H_
