/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_datectrl.cpp
// Purpose:     XML resource handler for wxDatePickerCtrl
// Author:      Vaclav Slavik
// Created:     2005-02-07
// Copyright:   (c) 2005 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_DATEPICKCTRL

#include "wx/xrc/xh_datectrl.h"
#include "wx/datectrl.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxDateCtrlXmlHandler, wxXmlResourceHandler);

wxDateCtrlXmlHandler::wxDateCtrlXmlHandler() : wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxDP_DEFAULT);
    XRC_ADD_STYLE(wxDP_SPIN);
    XRC_ADD_STYLE(wxDP_DROPDOWN);
    XRC_ADD_STYLE(wxDP_ALLOWNONE);
    XRC_ADD_STYLE(wxDP_SHOWCENTURY);
    AddWindowStyles();
}

wxObject *wxDateCtrlXmlHandler::DoCreateResource()
{
   XRC_MAKE_INSTANCE(picker, wxDatePickerCtrl)

   picker->Create(m_parentAsWindow,
                  GetID(),
                  wxDefaultDateTime,
                  GetPosition(), GetSize(),
                  GetStyle(wxT("style"), wxDP_DEFAULT | wxDP_SHOWCENTURY),
                  wxDefaultValidator,
                  GetName());

    SetupWindow(picker);

    return picker;
}

bool wxDateCtrlXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxDatePickerCtrl"));
}

#endif // wxUSE_XRC && wxUSE_DATEPICKCTRL
