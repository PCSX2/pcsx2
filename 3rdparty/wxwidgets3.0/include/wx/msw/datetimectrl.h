///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/datetimectrl.h
// Purpose:     wxDateTimePickerCtrl for Windows.
// Author:      Vadim Zeitlin
// Created:     2011-09-22 (extracted from wx/msw/datectrl.h).
// Copyright:   (c) 2005-2011 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_DATETIMECTRL_H_
#define _WX_MSW_DATETIMECTRL_H_

#include "wx/intl.h"

// Forward declare a struct from Platform SDK.
struct tagNMDATETIMECHANGE;

// ----------------------------------------------------------------------------
// wxDateTimePickerCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDateTimePickerCtrl : public wxDateTimePickerCtrlBase
{
public:
    // set/get the date
    virtual void SetValue(const wxDateTime& dt);
    virtual wxDateTime GetValue() const;

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

protected:
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }
    virtual wxSize DoGetBestSize() const;

    // Helper for the derived classes Create(): creates a native control with
    // the specified attributes.
    bool MSWCreateDateTimePicker(wxWindow *parent,
                                 wxWindowID id,
                                 const wxDateTime& dt,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style,
                                 const wxValidator& validator,
                                 const wxString& name);

    // Notice that the methods below must be overridden in all native MSW
    // classes inheriting from this one but they can't be pure virtual because
    // the generic implementations, not needing nor able to implement them, is
    // also derived from this class currently. The real problem is, of course,
    // this wrong class structure because the generic classes also inherit the
    // wrong implementations of Set/GetValue() and DoGetBestSize() but as they
    // override these methods anyhow, it does work -- but is definitely ugly
    // and need to be changed (but how?) in the future.

#if wxUSE_INTL
    // Override to return the date/time format used by this control.
    virtual wxLocaleInfo MSWGetFormat() const /* = 0 */
    {
        wxFAIL_MSG( "Unreachable" );
        return wxLOCALE_TIME_FMT;
    }
#endif // wxUSE_INTL

    // Override to indicate whether we can have no date at all.
    virtual bool MSWAllowsNone() const /* = 0 */
    {
        wxFAIL_MSG( "Unreachable" );
        return false;
    }

    // Override to update m_date and send the event when the control contents
    // changes, return true if the event was handled.
    virtual bool MSWOnDateTimeChange(const tagNMDATETIMECHANGE& dtch) /* = 0 */
    {
        wxUnusedVar(dtch);
        wxFAIL_MSG( "Unreachable" );
        return false;
    }


    // the date currently shown by the control, may be invalid
    wxDateTime m_date;
};

#endif // _WX_MSW_DATETIMECTRL_H_
