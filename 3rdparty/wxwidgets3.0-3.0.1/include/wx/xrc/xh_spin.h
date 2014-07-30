/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_spin.h
// Purpose:     XML resource handler for wxSpinButton and wxSpinCtrl
// Author:      Bob Mitchell
// Created:     2000/03/21
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_SPIN_H_
#define _WX_XH_SPIN_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC

#if wxUSE_SPINBTN

class WXDLLIMPEXP_XRC wxSpinButtonXmlHandler : public wxXmlResourceHandler
{
public:
    wxSpinButtonXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

    DECLARE_DYNAMIC_CLASS(wxSpinButtonXmlHandler)
};

#endif // wxUSE_SPINBTN


#if wxUSE_SPINCTRL

class WXDLLIMPEXP_XRC wxSpinCtrlXmlHandler : public wxXmlResourceHandler
{
public:
    wxSpinCtrlXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

    DECLARE_DYNAMIC_CLASS(wxSpinCtrlXmlHandler)
};

#endif // wxUSE_SPINCTRL

#endif // wxUSE_XRC

#endif // _WX_XH_SPIN_H_
