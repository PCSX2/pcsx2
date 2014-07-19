/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/msgdlg.h
// Purpose:     wxMessageDialog class. Use generic version if no
//              platform-specific implementation.
// Author:      David Webster
// Modified by:
// Created:     10/12/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSGBOXDLG_H_
#define _WX_MSGBOXDLG_H_

class WXDLLIMPEXP_CORE wxMessageDialog : public wxMessageDialogBase
{
public:
    wxMessageDialog( wxWindow*       pParent
                    ,const wxString& rsMessage
                    ,const wxString& rsCaption = wxMessageBoxCaptionStr
                    ,long            lStyle = wxOK|wxCENTRE
                    ,const wxPoint&  WXUNUSED(rPos) = wxDefaultPosition
                   )
        : wxMessageDialogBase(pParent, rsMessage, rsCaption, lStyle)
    {
    }

    int ShowModal(void);

protected:
    DECLARE_DYNAMIC_CLASS(wxMessageDialog)
}; // end of CLASS wxMessageDialog

#endif // _WX_MSGBOXDLG_H_
