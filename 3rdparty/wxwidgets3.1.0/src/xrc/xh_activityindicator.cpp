/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_activityindicator.cpp
// Purpose:     Implementation of wxActivityIndicator XRC handler.
// Author:      Vadim Zeitlin
// Created:     2015-03-18
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_ACTIVITYINDICATOR

#include "wx/xrc/xh_activityindicator.h"
#include "wx/activityindicator.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxActivityIndicatorXmlHandler, wxXmlResourceHandler);

wxActivityIndicatorXmlHandler::wxActivityIndicatorXmlHandler()
{
    AddWindowStyles();
}

wxObject *wxActivityIndicatorXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(ctrl, wxActivityIndicator)

    ctrl->Create(m_parentAsWindow,
                 GetID(),
                 GetPosition(), GetSize(),
                 GetStyle(),
                 GetName());

    SetupWindow(ctrl);

    if ( GetBool(wxS("running")) )
        ctrl->Start();

    return ctrl;
}

bool wxActivityIndicatorXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxS("wxActivityIndicator"));
}

#endif // wxUSE_XRC && wxUSE_ACTIVITYINDICATOR
