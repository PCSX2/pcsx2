///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/datectrl.h
// Purpose:     Declaration of wxOSX-specific wxDatePickerCtrl class.
// Author:      Vadim Zeitlin
// Created:     2011-12-18
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_DATECTRL_H_
#define _WX_OSX_DATECTRL_H_

// ----------------------------------------------------------------------------
// wxDatePickerCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDatePickerCtrl : public wxDatePickerCtrlBase
{
public:
    // Constructors.
    wxDatePickerCtrl() { }

    wxDatePickerCtrl(wxWindow *parent,
                     wxWindowID id,
                     const wxDateTime& dt = wxDefaultDateTime,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxDP_DEFAULT | wxDP_SHOWCENTURY,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxDatePickerCtrlNameStr)
    {
        Create(parent, id, dt, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& dt = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDP_DEFAULT | wxDP_SHOWCENTURY,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxDatePickerCtrlNameStr);

    // Implement the base class pure virtuals.
    virtual void SetRange(const wxDateTime& dt1, const wxDateTime& dt2);
    virtual bool GetRange(wxDateTime *dt1, wxDateTime *dt2) const;

    virtual void OSXGenerateEvent(const wxDateTime& dt);

private:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDatePickerCtrl);
};

#endif // _WX_OSX_DATECTRL_H_
