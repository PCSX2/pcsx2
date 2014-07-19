///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/private/datetime.h
// Purpose:
// Author:      Vadim Zeitlin
// Created:     2011-12-19
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_CORE_PRIVATE_DATETIMECTRL_H_
#define _WX_OSX_CORE_PRIVATE_DATETIMECTRL_H_

#if wxUSE_DATEPICKCTRL

#include "wx/osx/private.h"

#include "wx/datetime.h"

enum wxDateTimeWidgetKind
{
    wxDateTimeWidget_YearMonthDay,
    wxDateTimeWidget_HourMinuteSecond
};

// ----------------------------------------------------------------------------
// wxDateTimeWidgetImpl: peer class for wxDateTimePickerCtrl.
// ----------------------------------------------------------------------------

class wxDateTimeWidgetImpl
#if wxOSX_USE_COCOA
    : public wxWidgetCocoaImpl
#elif wxOSX_USE_CARBON
    : public wxMacControl
#else
    #error "Unsupported platform"
#endif
{
public:
    static wxDateTimeWidgetImpl*
    CreateDateTimePicker(wxDateTimePickerCtrl* wxpeer,
                         const wxDateTime& dt,
                         const wxPoint& pos,
                         const wxSize& size,
                         long style,
                         wxDateTimeWidgetKind kind);

    virtual void SetDateTime(const wxDateTime& dt) = 0;
    virtual wxDateTime GetDateTime() const = 0;

    virtual void SetDateRange(const wxDateTime& dt1, const wxDateTime& dt2) = 0;
    virtual bool GetDateRange(wxDateTime* dt1, wxDateTime* dt2) = 0;

    virtual ~wxDateTimeWidgetImpl() { }

protected:
#if wxOSX_USE_COCOA
    wxDateTimeWidgetImpl(wxDateTimePickerCtrl* wxpeer, WXWidget view)
        : wxWidgetCocoaImpl(wxpeer, view)
    {
    }
#elif wxOSX_USE_CARBON
    // There is no Carbon implementation of this control yet so we don't need
    // any ctor for it yet but it should be added here if Carbon version is
    // written later.
#endif
};

#endif // wxUSE_DATEPICKCTRL

#endif // _WX_OSX_CORE_PRIVATE_DATETIMECTRL_H_
