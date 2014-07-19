/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/button.h
// Purpose:     wxButton class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BUTTON_H_
#define _WX_BUTTON_H_

// Pushbutton
class WXDLLIMPEXP_CORE wxButton: public wxButtonBase
{
public:
    wxButton() { }
    wxButton(wxWindow *parent,
        wxWindowID id,
        const wxString& label = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxString& label = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxButtonNameStr);

    virtual wxWindow *SetDefault();
    virtual void Command(wxCommandEvent& event);

    static wxSize GetDefaultSize();

    // Implementation
    virtual wxSize GetMinSize() const;

protected:
    virtual wxSize DoGetBestSize() const;

private:
    wxSize OldGetBestSize() const;
    wxSize OldGetMinSize() const;
    void SetDefaultShadowThicknessAndResize();

    DECLARE_DYNAMIC_CLASS(wxButton)
};

#endif // _WX_BUTTON_H_
