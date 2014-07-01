/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_bmpbt.cpp
// Purpose:     XRC resource for bitmap buttons
// Author:      Brian Gavin
// Created:     2000/09/09
// Copyright:   (c) 2000 Brian Gavin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_BMPBUTTON

#include "wx/xrc/xh_bmpbt.h"

#ifndef WX_PRECOMP
    #include "wx/bmpbuttn.h"
#endif

IMPLEMENT_DYNAMIC_CLASS(wxBitmapButtonXmlHandler, wxXmlResourceHandler)

wxBitmapButtonXmlHandler::wxBitmapButtonXmlHandler()
: wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxBU_AUTODRAW);
    XRC_ADD_STYLE(wxBU_LEFT);
    XRC_ADD_STYLE(wxBU_RIGHT);
    XRC_ADD_STYLE(wxBU_TOP);
    XRC_ADD_STYLE(wxBU_BOTTOM);
    XRC_ADD_STYLE(wxBU_EXACTFIT);
    AddWindowStyles();
}

wxObject *wxBitmapButtonXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(button, wxBitmapButton)

    button->Create(m_parentAsWindow,
                   GetID(),
                   GetBitmap(wxT("bitmap"), wxART_BUTTON),
                   GetPosition(), GetSize(),
                   GetStyle(wxT("style"), wxBU_AUTODRAW),
                   wxDefaultValidator,
                   GetName());
    if (GetBool(wxT("default"), 0))
        button->SetDefault();
    SetupWindow(button);

    if (GetParamNode(wxT("selected")))
        button->SetBitmapSelected(GetBitmap(wxT("selected")));
    if (GetParamNode(wxT("focus")))
        button->SetBitmapFocus(GetBitmap(wxT("focus")));
    if (GetParamNode(wxT("disabled")))
        button->SetBitmapDisabled(GetBitmap(wxT("disabled")));
    if (GetParamNode(wxT("hover")))
        button->SetBitmapHover(GetBitmap(wxT("hover")));

    return button;
}

bool wxBitmapButtonXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxBitmapButton"));
}

#endif // wxUSE_XRC && wxUSE_BMPBUTTON
