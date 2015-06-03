///////////////////////////////////////////////////////////////////////////////
// Name:        wx/timectrl.h
// Purpose:     Declaration of wxDateTimePickerCtrl class.
// Author:      Vadim Zeitlin
// Created:     2011-09-22
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DATETIME_CTRL_H_
#define _WX_DATETIME_CTRL_H_

#include "wx/defs.h"

#if wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL

#define wxNEEDS_DATETIMEPICKCTRL

#include "wx/control.h"         // the base class

#include "wx/datetime.h"

// ----------------------------------------------------------------------------
// wxDateTimePickerCtrl: Private common base class of wx{Date,Time}PickerCtrl.
// ----------------------------------------------------------------------------

// This class is an implementation detail and should not be used directly, only
// use the documented API of wxDateTimePickerCtrl and wxTimePickerCtrl.
class WXDLLIMPEXP_ADV wxDateTimePickerCtrlBase : public wxControl
{
public:
    // Set/get the date or time (in the latter case, time part is ignored).
    virtual void SetValue(const wxDateTime& dt) = 0;
    virtual wxDateTime GetValue() const = 0;
};

#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    #include "wx/msw/datetimectrl.h"
#elif defined(__WXOSX_COCOA__) && !defined(__WXUNIVERSAL__)
    #include "wx/osx/datetimectrl.h"
#else
    typedef wxDateTimePickerCtrlBase wxDateTimePickerCtrl;
#endif

#endif // wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL

#endif // _WX_DATETIME_CTRL_H_
