/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/fdrepdlg.h
// Purpose:     wxGenericFindReplaceDialog class
// Author:      Markus Greither
// Modified by:
// Created:     25/05/2001
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_FDREPDLG_H_
#define _WX_GENERIC_FDREPDLG_H_

class WXDLLIMPEXP_FWD_CORE wxCheckBox;
class WXDLLIMPEXP_FWD_CORE wxRadioBox;
class WXDLLIMPEXP_FWD_CORE wxTextCtrl;

// ----------------------------------------------------------------------------
// wxGenericFindReplaceDialog: dialog for searching / replacing text
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericFindReplaceDialog : public wxFindReplaceDialogBase
{
public:
    wxGenericFindReplaceDialog() { Init(); }

    wxGenericFindReplaceDialog(wxWindow *parent,
                               wxFindReplaceData *data,
                               const wxString& title,
                               int style = 0)
    {
        Init();

        (void)Create(parent, data, title, style);
    }

    bool Create(wxWindow *parent,
                wxFindReplaceData *data,
                const wxString& title,
                int style = 0);

protected:
    void Init();

    void SendEvent(const wxEventType& evtType);

    void OnFind(wxCommandEvent& event);
    void OnReplace(wxCommandEvent& event);
    void OnReplaceAll(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

    void OnUpdateFindUI(wxUpdateUIEvent& event);

    void OnCloseWindow(wxCloseEvent& event);

    wxCheckBox *m_chkCase,
               *m_chkWord;

    wxRadioBox *m_radioDir;

    wxTextCtrl *m_textFind,
               *m_textRepl;

private:
    wxDECLARE_DYNAMIC_CLASS(wxGenericFindReplaceDialog);

    wxDECLARE_EVENT_TABLE();
};

#endif // _WX_GENERIC_FDREPDLG_H_
