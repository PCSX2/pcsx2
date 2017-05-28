///////////////////////////////////////////////////////////////////////////////
// Name:        wx/timectrl.h
// Purpose:     Declaration of wxTimePickerCtrl class.
// Author:      Vadim Zeitlin
// Created:     2011-09-22
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TIMECTRL_H_
#define _WX_TIMECTRL_H_

#include "wx/defs.h"

#if wxUSE_TIMEPICKCTRL

#include "wx/datetimectrl.h"

#define wxTimePickerCtrlNameStr wxS("timectrl")

// No special styles are currently defined for this control but still define a
// symbolic constant for the default style for consistency.
enum
{
    wxTP_DEFAULT = 0
};

// ----------------------------------------------------------------------------
// wxTimePickerCtrl: Allow the user to enter the time.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxTimePickerCtrlBase : public wxDateTimePickerCtrl
{
public:
    /*
        The derived classes should implement ctor and Create() method with the
        following signature:

        bool Create(wxWindow *parent,
                    wxWindowID id,
                    const wxDateTime& dt = wxDefaultDateTime,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxTP_DEFAULT,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxTimePickerCtrlNameStr);
     */

    /*
        We also inherit Set/GetValue() methods from the base class which define
        our public API. Notice that the date portion of the date passed as
        input or received as output is or should be ignored, only the time part
        of wxDateTime objects is really significant here. Use Set/GetTime()
        below for possibly simpler interface.
     */

    // Set the given time.
    bool SetTime(int hour, int min, int sec)
    {
        // Notice that we should use a date on which DST doesn't change to
        // avoid any problems with time discontinuity so use a fixed date (on
        // which nobody changes DST) instead of e.g. today.
        wxDateTime dt(1, wxDateTime::Jan, 2012, hour, min, sec);
        if ( !dt.IsValid() )
        {
            // No need to assert here, wxDateTime already does it for us.
            return false;
        }

        SetValue(dt);

        return true;
    }

    // Get the current time components. All pointers must be non-NULL.
    bool GetTime(int* hour, int* min, int* sec) const
    {
        wxCHECK_MSG( hour && min && sec, false,
                     wxS("Time component pointers must be non-NULL") );

        const wxDateTime::Tm tm = GetValue().GetTm();
        *hour = tm.hour;
        *min = tm.min;
        *sec = tm.sec;

        return true;
    }
};

#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    #include "wx/msw/timectrl.h"

    #define wxHAS_NATIVE_TIMEPICKERCTRL
#elif defined(__WXOSX_COCOA__) && !defined(__WXUNIVERSAL__)
    #include "wx/osx/timectrl.h"

    #define wxHAS_NATIVE_TIMEPICKERCTRL
#else
    #include "wx/generic/timectrl.h"

    class WXDLLIMPEXP_ADV wxTimePickerCtrl : public wxTimePickerCtrlGeneric
    {
    public:
        wxTimePickerCtrl() { }
        wxTimePickerCtrl(wxWindow *parent,
                         wxWindowID id,
                         const wxDateTime& date = wxDefaultDateTime,
                         const wxPoint& pos = wxDefaultPosition,
                         const wxSize& size = wxDefaultSize,
                         long style = wxTP_DEFAULT,
                         const wxValidator& validator = wxDefaultValidator,
                         const wxString& name = wxTimePickerCtrlNameStr)
            : wxTimePickerCtrlGeneric(parent, id, date, pos, size, style, validator, name)
        {
        }

    private:
        wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxTimePickerCtrl);
    };
#endif

#endif // wxUSE_TIMEPICKCTRL

#endif // _WX_TIMECTRL_H_
