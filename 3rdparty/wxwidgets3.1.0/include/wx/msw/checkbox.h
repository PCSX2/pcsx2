/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/checkbox.h
// Purpose:     wxCheckBox class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKBOX_H_
#define _WX_CHECKBOX_H_

#include "wx/msw/ownerdrawnbutton.h"

// Checkbox item (single checkbox)
class WXDLLIMPEXP_CORE wxCheckBox : public wxMSWOwnerDrawnButton<wxCheckBoxBase>
{
public:
    wxCheckBox() { }
    wxCheckBox(wxWindow *parent,
               wxWindowID id,
               const wxString& label,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxCheckBoxNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxCheckBoxNameStr);

    virtual void SetValue(bool value);
    virtual bool GetValue() const;

    // override some base class virtuals
    virtual void SetLabel(const wxString& label);

    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual void Command(wxCommandEvent& event);

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

    // implementation only from now on
    virtual WXDWORD MSWGetStyle(long flags, WXDWORD *exstyle = NULL) const;

protected:
    virtual wxSize DoGetBestClientSize() const;

    virtual void DoSet3StateValue(wxCheckBoxState value);
    virtual wxCheckBoxState DoGet3StateValue() const;

    // Implement wxMSWOwnerDrawnButtonBase methods.
    virtual int MSWGetButtonStyle() const wxOVERRIDE;
    virtual void MSWOnButtonResetOwnerDrawn() wxOVERRIDE;
    virtual int MSWGetButtonCheckedFlag() const wxOVERRIDE;
    virtual void
        MSWDrawButtonBitmap(wxDC& dc, const wxRect& rect, int flags) wxOVERRIDE;

private:
    // common part of all ctors
    void Init();

    // current state of the checkbox
    wxCheckBoxState m_state;

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxCheckBox);
};

#endif // _WX_CHECKBOX_H_
