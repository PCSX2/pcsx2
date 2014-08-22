/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_text.h
// Purpose:     XML resource handler for wxTextCtrl
// Author:      Aleksandras Gluchovas
// Created:     2000/03/21
// Copyright:   (c) 2000 Aleksandras Gluchovas
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_TEXT_H_
#define _WX_XH_TEXT_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_TEXTCTRL

class WXDLLIMPEXP_XRC wxTextCtrlXmlHandler : public wxXmlResourceHandler
{
    DECLARE_DYNAMIC_CLASS(wxTextCtrlXmlHandler)

public:
    wxTextCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);
};

#endif // wxUSE_XRC && wxUSE_TEXTCTRL

#endif // _WX_XH_TEXT_H_
