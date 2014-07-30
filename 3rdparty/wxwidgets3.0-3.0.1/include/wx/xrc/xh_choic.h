/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xrc/xh_choic.h
// Purpose:     XML resource handler for wxChoice
// Author:      Bob Mitchell
// Created:     2000/03/21
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XH_CHOIC_H_
#define _WX_XH_CHOIC_H_

#include "wx/xrc/xmlres.h"

#if wxUSE_XRC && wxUSE_CHOICE

class WXDLLIMPEXP_XRC wxChoiceXmlHandler : public wxXmlResourceHandler
{
    DECLARE_DYNAMIC_CLASS(wxChoiceXmlHandler)

public:
    wxChoiceXmlHandler();
    virtual wxObject *DoCreateResource();
    virtual bool CanHandle(wxXmlNode *node);

private:
    bool m_insideBox;
    wxArrayString strList;
};

#endif // wxUSE_XRC && wxUSE_CHOICE

#endif // _WX_XH_CHOIC_H_
