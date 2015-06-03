/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextdialogpage.h
// Purpose:     Formatting dialog page base class for wxRTC
// Author:      Julian Smart
// Modified by:
// Created:     2010-11-14
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RICHTEXTDIALOGPAGE_H_
#define _WX_RICHTEXTDIALOGPAGE_H_

#if wxUSE_RICHTEXT

#include "wx/panel.h"
#include "wx/richtext/richtextuicustomization.h"

/**
    @class wxRichTextDialogPage
    The base class for formatting dialog pages.
 **/

class WXDLLIMPEXP_RICHTEXT wxRichTextDialogPage: public wxPanel
{
public:
    wxDECLARE_CLASS(wxRichTextDialogPage);
    wxRichTextDialogPage() {}
    wxRichTextDialogPage(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0)
    {
        Create(parent, id, pos, size, style);
    }

    DECLARE_BASE_CLASS_HELP_PROVISION()
};

#endif
    // wxUSE_RICHTEXT

#endif
    // _WX_RICHTEXTDIALOGPAGE_H_
