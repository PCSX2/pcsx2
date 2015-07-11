///////////////////////////////////////////////////////////////////////////////
// Name:        wx/dateevt.h
// Purpose:     declares wxDateEvent class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-01-10
// Copyright:   (c) 2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DATEEVT_H_
#define _WX_DATEEVT_H_

#include "wx/event.h"
#include "wx/datetime.h"
#include "wx/window.h"

// ----------------------------------------------------------------------------
// wxDateEvent: used by wxCalendarCtrl, wxDatePickerCtrl and wxTimePickerCtrl.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDateEvent : public wxCommandEvent
{
public:
    wxDateEvent() { }
    wxDateEvent(wxWindow *win, const wxDateTime& dt, wxEventType type)
        : wxCommandEvent(type, win->GetId()),
          m_date(dt)
    {
        SetEventObject(win);
    }

    const wxDateTime& GetDate() const { return m_date; }
    void SetDate(const wxDateTime &date) { m_date = date; }

    // default copy ctor, assignment operator and dtor are ok
    virtual wxEvent *Clone() const { return new wxDateEvent(*this); }

private:
    wxDateTime m_date;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxDateEvent)
};

// ----------------------------------------------------------------------------
// event types and macros for handling them
// ----------------------------------------------------------------------------

wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_ADV, wxEVT_DATE_CHANGED, wxDateEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_ADV, wxEVT_TIME_CHANGED, wxDateEvent);

typedef void (wxEvtHandler::*wxDateEventFunction)(wxDateEvent&);

#define wxDateEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxDateEventFunction, func)

#define EVT_DATE_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_DATE_CHANGED, id, wxDateEventHandler(fn))

#define EVT_TIME_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_TIME_CHANGED, id, wxDateEventHandler(fn))

#endif // _WX_DATEEVT_H_

