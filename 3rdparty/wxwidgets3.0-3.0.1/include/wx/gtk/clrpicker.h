/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/clrpicker.h
// Purpose:     wxColourButton header
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_CLRPICKER_H_
#define _WX_GTK_CLRPICKER_H_

#include "wx/button.h"

//-----------------------------------------------------------------------------
// wxColourButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxColourButton : public wxButton,
                                        public wxColourPickerWidgetBase
{
public:
    wxColourButton() : m_topParent(NULL) {}
    wxColourButton(wxWindow *parent,
                   wxWindowID id,
                   const wxColour& initial = *wxBLACK,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxCLRBTN_DEFAULT_STYLE,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxColourPickerWidgetNameStr)
        : m_topParent(NULL)
    {
        Create(parent, id, initial, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxColour& initial = *wxBLACK,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxCLRBTN_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxColourPickerWidgetNameStr);

    virtual ~wxColourButton();

protected:
    void UpdateColour();

public:     // used by the GTK callback only

    void GTKSetColour(const wxColour& colour)
        { m_colour = colour; }

    wxWindow *m_topParent;

private:
    DECLARE_DYNAMIC_CLASS(wxColourButton)
};

#endif // _WX_GTK_CLRPICKER_H_

