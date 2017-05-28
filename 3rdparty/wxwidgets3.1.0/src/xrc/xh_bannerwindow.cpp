///////////////////////////////////////////////////////////////////////////////
// Name:        srx/xrc/xh_bannerwindow.h
// Purpose:     Implementation of wxBannerWindow XRC handler.
// Author:      Vadim Zeitlin
// Created:     2011-08-16
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_BANNERWINDOW

#include "wx/xrc/xh_bannerwindow.h"
#include "wx/bannerwindow.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxBannerWindowXmlHandler, wxXmlResourceHandler);

wxBannerWindowXmlHandler::wxBannerWindowXmlHandler()
    : wxXmlResourceHandler()
{
    AddWindowStyles();
}

wxObject *wxBannerWindowXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(banner, wxBannerWindow)

    banner->Create(m_parentAsWindow,
                   GetID(),
                   GetDirection(wxS("direction")),
                   GetPosition(),
                   GetSize(),
                   GetStyle(wxS("style")),
                   GetName());

    SetupWindow(banner);

    const wxColour colStart = GetColour(wxS("gradient-start"));
    const wxColour colEnd = GetColour(wxS("gradient-end"));
    if ( colStart.IsOk() || colEnd.IsOk() )
    {
        if ( !colStart.IsOk() || !colEnd.IsOk() )
        {
            ReportError
            (
                "Both start and end gradient colours must be "
                "specified if either one is."
            );
        }
        else
        {
            banner->SetGradient(colStart, colEnd);
        }
    }

    wxBitmap bitmap = GetBitmap();
    if ( bitmap.IsOk() )
    {
        if ( colStart.IsOk() || colEnd.IsOk() )
        {
            ReportError
            (
                "Gradient colours are ignored by wxBannerWindow "
                "if the background bitmap is specified."
            );
        }

        banner->SetBitmap(bitmap);
    }

    banner->SetText(GetText(wxS("title")), GetText(wxS("message")));

    return banner;
}

bool wxBannerWindowXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxS("wxBannerWindow"));
}

#endif // wxUSE_XRC && wxUSE_BANNERWINDOW
