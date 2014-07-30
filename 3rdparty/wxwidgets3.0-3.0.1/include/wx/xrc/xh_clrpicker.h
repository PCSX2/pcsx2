/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_clrpicker.h
// Purpose:     XML resource handler for wxColourPickerCtrl
// Author:      Francesco Montorsi
// Created:     2006-04-17
// Copyright:   (c) 2006 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_CLRPICKERCTRL_H_
#define _WX_XH_CLRPICKERCTRL_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_COLOURPICKERCTRL

class WXDLLIMPEXP_XRC wxColourPickerCtrlXmlHandler : public wxXmlResourceHandler
{
    DECLARE_DYNAMIC_CLASS(wxColourPickerCtrlXmlHandler)

public:
    wxColourPickerCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};

#endif // wxUSE_XRC && wxUSE_COLOURPICKERCTRL

#endif // _WX_XH_CLRPICKERCTRL_H_
