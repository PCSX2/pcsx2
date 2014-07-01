/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_tglbtn.cpp
// Purpose:     XRC resource for wxToggleButton
// Author:      Bob Mitchell
// Created:     2000/03/21
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_TOGGLEBTN

#include "wx/xrc/xh_tglbtn.h"
#include "wx/tglbtn.h"
#include "wx/button.h" // solely for wxBU_EXACTFIT

IMPLEMENT_DYNAMIC_CLASS(wxToggleButtonXmlHandler, wxXmlResourceHandler)

wxToggleButtonXmlHandler::wxToggleButtonXmlHandler()
    : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxBU_EXACTFIT);

    AddWindowStyles();
}

wxObject *wxToggleButtonXmlHandler::DoCreateResource()
{

   wxObject *control = m_instance;

#if !defined(__WXUNIVERSAL__) && !defined(__WXMOTIF__) && !defined(__WXPM__) && !(defined(__WXGTK__) && !defined(__WXGTK20__))

    if (m_class == wxT("wxBitmapToggleButton"))
    {
       if (!control)
           control = new wxBitmapToggleButton;

        DoCreateBitmapToggleButton(control);
    }
    else
#endif
    {
       if (!control)
           control = new wxToggleButton;

        DoCreateToggleButton(control);
    }

    SetupWindow(wxDynamicCast(control, wxWindow));

    return control;
}

bool wxToggleButtonXmlHandler::CanHandle(wxXmlNode *node)
{
    return (
               IsOfClass(node, wxT("wxToggleButton")) ||
               IsOfClass(node, wxT("wxBitmapToggleButton"))
           );
}

void wxToggleButtonXmlHandler::DoCreateToggleButton(wxObject *control)
{
    wxToggleButton *button = wxDynamicCast(control, wxToggleButton);

    button->Create(m_parentAsWindow,
                   GetID(),
                   GetText(wxT("label")),
                   GetPosition(), GetSize(),
                   GetStyle(),
                   wxDefaultValidator,
                   GetName());

    button->SetValue(GetBool( wxT("checked")));
}

#if !defined(__WXUNIVERSAL__) && !defined(__WXMOTIF__) && !defined(__WXPM__) && !(defined(__WXGTK__) && !defined(__WXGTK20__))
void wxToggleButtonXmlHandler::DoCreateBitmapToggleButton(wxObject *control)
{
    wxBitmapToggleButton *button = wxDynamicCast(control, wxBitmapToggleButton);

    button->Create(m_parentAsWindow,
                   GetID(),
                   GetBitmap(wxT("bitmap"), wxART_BUTTON),
                   GetPosition(), GetSize(),
                   GetStyle(),
                   wxDefaultValidator,
                   GetName());

    button->SetValue(GetBool( wxT("checked")));
}
#endif
#endif // wxUSE_XRC && wxUSE_TOGGLEBTN
