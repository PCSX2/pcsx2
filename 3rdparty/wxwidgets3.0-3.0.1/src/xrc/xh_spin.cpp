/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_spin.cpp
// Purpose:     XRC resource for wxSpinButton
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

#if wxUSE_XRC

#include "wx/xrc/xh_spin.h"

#if wxUSE_SPINBTN

#include "wx/spinbutt.h"

static const long DEFAULT_VALUE = 0;
static const long DEFAULT_MIN = 0;
static const long DEFAULT_MAX = 100;

IMPLEMENT_DYNAMIC_CLASS(wxSpinButtonXmlHandler, wxXmlResourceHandler)

wxSpinButtonXmlHandler::wxSpinButtonXmlHandler()
: wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxSP_HORIZONTAL);
    XRC_ADD_STYLE(wxSP_VERTICAL);
    XRC_ADD_STYLE(wxSP_ARROW_KEYS);
    XRC_ADD_STYLE(wxSP_WRAP);
    AddWindowStyles();
}

wxObject *wxSpinButtonXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(control, wxSpinButton)

    control->Create(m_parentAsWindow,
                    GetID(),
                    GetPosition(), GetSize(),
                    GetStyle(wxT("style"), wxSP_VERTICAL | wxSP_ARROW_KEYS),
                    GetName());

    control->SetValue(GetLong( wxT("value"), DEFAULT_VALUE));
    control->SetRange(GetLong( wxT("min"), DEFAULT_MIN),
                      GetLong(wxT("max"), DEFAULT_MAX));
    SetupWindow(control);

    return control;
}

bool wxSpinButtonXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxSpinButton"));
}

#endif // wxUSE_SPINBTN

#if wxUSE_SPINCTRL

#include "wx/spinctrl.h"

IMPLEMENT_DYNAMIC_CLASS(wxSpinCtrlXmlHandler, wxXmlResourceHandler)

wxSpinCtrlXmlHandler::wxSpinCtrlXmlHandler()
: wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxSP_HORIZONTAL);
    XRC_ADD_STYLE(wxSP_VERTICAL);
    XRC_ADD_STYLE(wxSP_ARROW_KEYS);
    XRC_ADD_STYLE(wxSP_WRAP);
    XRC_ADD_STYLE(wxALIGN_LEFT);
    XRC_ADD_STYLE(wxALIGN_CENTER);
    XRC_ADD_STYLE(wxALIGN_RIGHT);
}

wxObject *wxSpinCtrlXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(control, wxSpinCtrl)

    control->Create(m_parentAsWindow,
                    GetID(),
                    GetText(wxT("value")),
                    GetPosition(), GetSize(),
                    GetStyle(wxT("style"), wxSP_ARROW_KEYS | wxALIGN_RIGHT),
                    GetLong(wxT("min"), DEFAULT_MIN),
                    GetLong(wxT("max"), DEFAULT_MAX),
                    GetLong(wxT("value"), DEFAULT_VALUE),
                    GetName());

    const long base = GetLong(wxS("base"), 10);
    if ( base != 10 )
        control->SetBase(base);

    SetupWindow(control);

    return control;
}

bool wxSpinCtrlXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxSpinCtrl"));
}

#endif // wxUSE_SPINCTRL

#endif // wxUSE_XRC
