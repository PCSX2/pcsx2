/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_comboctrl.cpp
// Purpose:     XRC resource for wxComboCtrl
// Author:      Jaakko Salli
// Created:     2009/01/25
// Copyright:   (c) 2009 Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_COMBOCTRL

#include "wx/xrc/xh_comboctrl.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/textctrl.h"    // for wxTE_PROCESS_ENTER
#endif

#include "wx/combo.h"


wxIMPLEMENT_DYNAMIC_CLASS(wxComboCtrlXmlHandler, wxXmlResourceHandler);

wxComboCtrlXmlHandler::wxComboCtrlXmlHandler()
                     : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxCB_SORT);
    XRC_ADD_STYLE(wxCB_READONLY);
    XRC_ADD_STYLE(wxTE_PROCESS_ENTER);
    XRC_ADD_STYLE(wxCC_SPECIAL_DCLICK);
    XRC_ADD_STYLE(wxCC_STD_BUTTON);
    AddWindowStyles();
}

wxObject *wxComboCtrlXmlHandler::DoCreateResource()
{
    if( m_class == wxT("wxComboCtrl"))
    {
        XRC_MAKE_INSTANCE(control, wxComboCtrl)

        control->Create(m_parentAsWindow,
                        GetID(),
                        GetText(wxT("value")),
                        GetPosition(), GetSize(),
                        GetStyle(),
                        wxDefaultValidator,
                        GetName());

        SetupWindow(control);

        return control;
    }
    return NULL;
}

bool wxComboCtrlXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxComboCtrl"));
}

#endif // wxUSE_XRC && wxUSE_COMBOBOX
