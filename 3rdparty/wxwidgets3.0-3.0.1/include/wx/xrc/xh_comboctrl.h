/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_comboctrl.h
// Purpose:     XML resource handler for wxComboBox
// Author:      Jaakko Salli
// Created:     2009/01/25
// Copyright:   (c) 2009 Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_COMBOCTRL_H_
#define _WX_XH_COMBOCTRL_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_COMBOCTRL

class WXDLLIMPEXP_XRC wxComboCtrlXmlHandler : public wxXmlResourceHandler
{
    DECLARE_DYNAMIC_CLASS(wxComboCtrlXmlHandler)

public:
    wxComboCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

private:
};

#endif // wxUSE_XRC && wxUSE_COMBOCTRL

#endif // _WX_XH_COMBOCTRL_H_
