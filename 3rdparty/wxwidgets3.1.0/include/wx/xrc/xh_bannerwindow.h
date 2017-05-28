///////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_bannerwindow.h
// Purpose:     Declaration of wxBannerWindow XRC handler.
// Author:      Vadim Zeitlin
// Created:     2011-08-16
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_BANNERWINDOW_H_
#define _WX_XH_BANNERWINDOW_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_BANNERWINDOW

class WXDLLIMPEXP_XRC wxBannerWindowXmlHandler : public wxXmlResourceHandler
{
public:
    wxBannerWindowXmlHandler();

    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

    wxDECLARE_DYNAMIC_CLASS(wxBannerWindowXmlHandler);
};

#endif // wxUSE_XRC && wxUSE_BANNERWINDOW

#endif // _WX_XH_BANNERWINDOW_H_
