/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_filectrl.cpp
// Purpose:     XML resource handler for wxFileCtrl
// Author:      Kinaou Hervé
// Created:     2009-05-11
// Copyright:   (c) 2009 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_FILECTRL

#include "wx/xrc/xh_filectrl.h"
#include "wx/filectrl.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxFileCtrlXmlHandler, wxXmlResourceHandler);

wxFileCtrlXmlHandler::wxFileCtrlXmlHandler() : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxFC_DEFAULT_STYLE);
    XRC_ADD_STYLE(wxFC_OPEN);
    XRC_ADD_STYLE(wxFC_SAVE);
    XRC_ADD_STYLE(wxFC_MULTIPLE);
    XRC_ADD_STYLE(wxFC_NOSHOWHIDDEN);

    AddWindowStyles();
}

wxObject *wxFileCtrlXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(filectrl, wxFileCtrl)

    filectrl->Create(m_parentAsWindow,
                     GetID(),
                     GetText(wxT("defaultdirectory")),
                     GetText(wxT("defaultfilename")),
                     GetParamValue(wxT("wildcard")),
                     GetStyle(wxT("style"), wxFC_DEFAULT_STYLE),
                     GetPosition(),
                     GetSize(),
                     GetName());

    SetupWindow(filectrl);
    return filectrl;
}

bool wxFileCtrlXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxFileCtrl"));
}

#endif // wxUSE_XRC && wxUSE_FILECTRL
