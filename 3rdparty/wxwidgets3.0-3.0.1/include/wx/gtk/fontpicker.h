/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/fontpicker.h
// Purpose:     wxFontButton header
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FONTPICKER_H_
#define _WX_GTK_FONTPICKER_H_

#include "wx/button.h"

//-----------------------------------------------------------------------------
// wxFontButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFontButton : public wxButton,
                                      public wxFontPickerWidgetBase
{
public:
    wxFontButton() {}
    wxFontButton(wxWindow *parent,
                 wxWindowID id,
                 const wxFont& initial = wxNullFont,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxFONTBTN_DEFAULT_STYLE,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString& name = wxFontPickerWidgetNameStr)
    {
       Create(parent, id, initial, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxFont& initial = wxNullFont,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxFONTBTN_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxFontPickerWidgetNameStr);

    virtual ~wxFontButton();

protected:
    void UpdateFont();


public:     // used by the GTK callback only

    void SetNativeFontInfo(const char *gtkdescription)
        { m_selectedFont.SetNativeFontInfo(wxString::FromAscii(gtkdescription)); }

private:
    DECLARE_DYNAMIC_CLASS(wxFontButton)
};

#endif // _WX_GTK_FONTPICKER_H_

