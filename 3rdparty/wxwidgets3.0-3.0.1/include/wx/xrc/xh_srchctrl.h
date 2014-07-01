/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_srchctl.h
// Purpose:     XRC resource handler for wxSearchCtrl
// Author:      Sander Berents
// Created:     2007/07/12
// Copyright:   (c) 2007 Sander Berents
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_SRCH_H_
#define _WX_XH_SRCH_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_SEARCHCTRL

class WXDLLIMPEXP_XRC wxSearchCtrlXmlHandler : public wxXmlResourceHandler
{
public:
    wxSearchCtrlXmlHandler();

    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

    DECLARE_DYNAMIC_CLASS(wxSearchCtrlXmlHandler)
};

#endif // wxUSE_XRC && wxUSE_SEARCHCTRL

#endif // _WX_XH_SRCH_H_
