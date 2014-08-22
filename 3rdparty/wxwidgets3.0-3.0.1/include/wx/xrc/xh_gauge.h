/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_gauge.h
// Purpose:     XML resource handler for wxGauge
// Author:      Bob Mitchell
// Created:     2000/03/21
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_GAUGE_H_
#define _WX_XH_GAUGE_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_GAUGE

class WXDLLIMPEXP_XRC wxGaugeXmlHandler : public wxXmlResourceHandler
{
public:
    wxGaugeXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

    DECLARE_DYNAMIC_CLASS(wxGaugeXmlHandler)
};

#endif // wxUSE_XRC && wxUSE_GAUGE

#endif // _WX_XH_GAUGE_H_
