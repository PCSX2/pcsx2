/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/private/timer.h
// Purpose:     wxTimer class
// Author:      David Webster
// Modified by:
// Created:     10/17/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_PRIVATE_TIMER_H_
#define _WX_OS2_PRIVATE_TIMER_H_

#include "wx/private/timer.h"

class WXDLLIMPEXP_CORE wxOS2TimerImpl: public wxTimerImpl
{
friend void wxProcessTimer(wxOS2TimerImpl& timer);

public:
    wxOS2TimerImpl(wxTimer *timer) : wxTimerImpl(timer) { m_ulId = 0; }

    virtual bool Start(int  nMilliseconds = -1, bool bOneShot = FALSE);
    virtual void Stop(void);

    virtual bool IsRunning(void) const { return m_ulId != 0L; }

protected:
    ULONG                            m_ulId;
    HAB                              m_Hab;
};

extern ULONG wxTimerProc( HWND  WXUNUSED(hwnd)
                         ,ULONG
                         ,int   nIdTimer
                         ,ULONG
                        );
#endif // _WX_OS2_PRIVATE_TIMER_H_
