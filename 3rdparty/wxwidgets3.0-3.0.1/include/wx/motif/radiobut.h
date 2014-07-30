/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/radiobut.h
// Purpose:     wxRadioButton class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RADIOBUT_H_
#define _WX_RADIOBUT_H_

class WXDLLIMPEXP_CORE wxRadioButton: public wxControl
{
    DECLARE_DYNAMIC_CLASS(wxRadioButton)
public:
    wxRadioButton();
    virtual ~wxRadioButton() { RemoveFromCycle(); }

    inline wxRadioButton(wxWindow *parent, wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxRadioButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxRadioButtonNameStr);

    virtual void SetValue(bool val);
    virtual bool GetValue() const ;

    void Command(wxCommandEvent& event);

    // Implementation
    virtual void ChangeBackgroundColour();

    // *this function is an implementation detail*
    // clears the selection in the radiobuttons in the cycle
    // and returns the old selection (if any)
    wxRadioButton* ClearSelections();
protected:
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }
private:
    wxRadioButton* AddInCycle(wxRadioButton* cycle);
    void RemoveFromCycle();
    wxRadioButton* NextInCycle() { return m_cycle; }

    wxRadioButton *m_cycle;
};

#endif
// _WX_RADIOBUT_H_
