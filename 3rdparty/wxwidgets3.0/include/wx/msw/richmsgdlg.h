/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/richmsgdlg.h
// Purpose:     wxRichMessageDialog
// Author:      Rickard Westerlund
// Created:     2010-07-04
// Copyright:   (c) 2010 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_RICHMSGDLG_H_
#define _WX_MSW_RICHMSGDLG_H_

class WXDLLIMPEXP_CORE wxRichMessageDialog : public wxGenericRichMessageDialog
{
public:
    wxRichMessageDialog(wxWindow *parent,
                        const wxString& message,
                        const wxString& caption = wxMessageBoxCaptionStr,
                        long style = wxOK | wxCENTRE)
        : wxGenericRichMessageDialog(parent, message, caption, style)
        { }

    // overridden base class method showing the native task dialog if possible
    virtual int ShowModal();

private:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxRichMessageDialog);
};

#endif // _WX_MSW_RICHMSGDLG_H_
