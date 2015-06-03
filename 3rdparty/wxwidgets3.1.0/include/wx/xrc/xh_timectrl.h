/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_timectrl.h
// Purpose:     XML resource handler for wxTimePickerCtrl
// Author:      Vadim Zeitlin
// Created:     2011-09-22
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_TIMECTRL_H_
#define _WX_XH_TIMECTRL_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_TIMEPICKCTRL

class WXDLLIMPEXP_XRC wxTimeCtrlXmlHandler : public wxXmlResourceHandler
{
public:
    wxTimeCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

private:
    wxDECLARE_DYNAMIC_CLASS(wxTimeCtrlXmlHandler);
};

#endif // wxUSE_XRC && wxUSE_TIMEPICKCTRL

#endif // _WX_XH_TIMECTRL_H_
