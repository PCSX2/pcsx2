/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/private/timer.h
// Purpose:     wxTimer class based on core foundation
// Author:      Stefan Csomor
// Created:     2008-07-16
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_CORE_PRIVATE_TIMER_H_
#define _WX_OSX_CORE_PRIVATE_TIMER_H_

#include "wx/private/timer.h"

struct wxOSXTimerInfo;

class WXDLLIMPEXP_CORE wxOSXTimerImpl : public wxTimerImpl
{
public:
    wxOSXTimerImpl(wxTimer *timer);
    virtual ~wxOSXTimerImpl();

    virtual bool Start(int milliseconds = -1, bool one_shot = false);
    virtual void Stop();

    virtual bool IsRunning() const;

private:
    wxOSXTimerInfo *m_info;
};

#endif // _WX_OSX_CORE_PRIVATE_TIMER_H_
