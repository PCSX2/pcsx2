///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/timectrl.h
// Purpose:     Generic implementation of wxTimePickerCtrl.
// Author:      Paul Breen, Vadim Zeitlin
// Created:     2011-09-22
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_TIMECTRL_H_
#define _WX_GENERIC_TIMECTRL_H_

#include "wx/containr.h"
#include "wx/compositewin.h"

class WXDLLIMPEXP_ADV wxTimePickerCtrlGeneric
    : public wxCompositeWindow< wxNavigationEnabled<wxTimePickerCtrlBase> >
{
public:
    typedef wxCompositeWindow< wxNavigationEnabled<wxTimePickerCtrlBase> > Base;

    // Creating the control.
    wxTimePickerCtrlGeneric() { Init(); }
    virtual ~wxTimePickerCtrlGeneric();
    wxTimePickerCtrlGeneric(wxWindow *parent,
                            wxWindowID id,
                            const wxDateTime& date = wxDefaultDateTime,
                            const wxPoint& pos = wxDefaultPosition,
                            const wxSize& size = wxDefaultSize,
                            long style = wxTP_DEFAULT,
                            const wxValidator& validator = wxDefaultValidator,
                            const wxString& name = wxTimePickerCtrlNameStr)
    {
        Init();

        (void)Create(parent, id, date, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& date = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTP_DEFAULT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTimePickerCtrlNameStr);

    // Implement pure virtual wxTimePickerCtrlBase methods.
    virtual void SetValue(const wxDateTime& date);
    virtual wxDateTime GetValue() const;

protected:
    virtual wxSize DoGetBestSize() const;

    virtual void DoMoveWindow(int x, int y, int width, int height);

private:
    void Init();

    // Return the list of the windows composing this one.
    virtual wxWindowList GetCompositeWindowParts() const;

    // Implementation data.
    class wxTimePickerGenericImpl* m_impl;

    wxDECLARE_NO_COPY_CLASS(wxTimePickerCtrlGeneric);
};

#endif // _WX_GENERIC_TIMECTRL_H_
