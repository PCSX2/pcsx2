/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/fontpickerg.h
// Purpose:     wxGenericFontButton header
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONTPICKER_H_
#define _WX_FONTPICKER_H_

#include "wx/button.h"
#include "wx/fontdata.h"

//-----------------------------------------------------------------------------
// wxGenericFontButton: a button which brings up a wxFontDialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericFontButton : public wxButton,
                                             public wxFontPickerWidgetBase
{
public:
    wxGenericFontButton() {}
    wxGenericFontButton(wxWindow *parent,
                        wxWindowID id,
                        const wxFont &initial = wxNullFont,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxFONTBTN_DEFAULT_STYLE,
                        const wxValidator& validator = wxDefaultValidator,
                        const wxString& name = wxFontPickerWidgetNameStr)
    {
        Create(parent, id, initial, pos, size, style, validator, name);
    }

    virtual wxColour GetSelectedColour() const
        { return m_data.GetColour(); }

    virtual void SetSelectedColour(const wxColour &colour)
        { m_data.SetColour(colour); UpdateFont(); }

    virtual ~wxGenericFontButton() {}


public:     // API extensions specific for wxGenericFontButton

    // user can override this to init font data in a different way
    virtual void InitFontData();

    // returns the font data shown in wxFontDialog
    wxFontData *GetFontData() { return &m_data; }


public:

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxFont &initial = *wxNORMAL_FONT,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxFONTBTN_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxFontPickerWidgetNameStr);

    void OnButtonClick(wxCommandEvent &);


protected:

    void UpdateFont();

    wxFontData m_data;

private:
    wxDECLARE_DYNAMIC_CLASS(wxGenericFontButton);
};


#endif // _WX_FONTPICKER_H_
