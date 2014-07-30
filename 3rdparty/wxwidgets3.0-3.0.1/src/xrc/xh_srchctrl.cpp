/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_srchctl.cpp
// Purpose:     XRC resource handler for wxSearchCtrl
// Author:      Sander Berents
// Created:     2007/07/12
// Copyright:   (c) 2007 Sander Berents
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_SEARCHCTRL

#include "wx/xrc/xh_srchctrl.h"
#include "wx/srchctrl.h"

IMPLEMENT_DYNAMIC_CLASS(wxSearchCtrlXmlHandler, wxXmlResourceHandler)

wxSearchCtrlXmlHandler::wxSearchCtrlXmlHandler() : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxTE_PROCESS_ENTER);
    XRC_ADD_STYLE(wxTE_PROCESS_TAB);
    XRC_ADD_STYLE(wxTE_NOHIDESEL);
    XRC_ADD_STYLE(wxTE_LEFT);
    XRC_ADD_STYLE(wxTE_CENTRE);
    XRC_ADD_STYLE(wxTE_RIGHT);
    XRC_ADD_STYLE(wxTE_CAPITALIZE);

    AddWindowStyles();
}

wxObject *wxSearchCtrlXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(ctrl, wxSearchCtrl)

    ctrl->Create(m_parentAsWindow,
                 GetID(),
                 GetText(wxT("value")),
                 GetPosition(),
                 GetSize(),
                 GetStyle(wxT("style"), wxTE_LEFT),
                 wxDefaultValidator,
                 GetName());

    SetupWindow(ctrl);

    return ctrl;
}

bool wxSearchCtrlXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxSearchCtrl"));
}

#endif // wxUSE_XRC && wxUSE_SEARCHCTRL
