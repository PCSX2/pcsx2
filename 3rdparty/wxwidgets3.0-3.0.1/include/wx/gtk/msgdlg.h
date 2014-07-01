/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/msgdlg.h
// Purpose:     wxMessageDialog for GTK+2
// Author:      Vaclav Slavik
// Modified by:
// Created:     2003/02/28
// Copyright:   (c) Vaclav Slavik, 2003
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_MSGDLG_H_
#define _WX_GTK_MSGDLG_H_

class WXDLLIMPEXP_CORE wxMessageDialog : public wxMessageDialogBase
{
public:
    wxMessageDialog(wxWindow *parent, const wxString& message,
                    const wxString& caption = wxMessageBoxCaptionStr,
                    long style = wxOK|wxCENTRE,
                    const wxPoint& pos = wxDefaultPosition);

    virtual int ShowModal();
    virtual bool Show(bool WXUNUSED(show) = true) { return false; }

protected:
    // implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) {}
    // override to convert wx mnemonics to GTK+ ones and handle stock ids
    virtual void DoSetCustomLabel(wxString& var, const ButtonLabel& label);

private:
    // override to use stock GTK+ defaults instead of just string ones
    virtual wxString GetDefaultYesLabel() const;
    virtual wxString GetDefaultNoLabel() const;
    virtual wxString GetDefaultOKLabel() const;
    virtual wxString GetDefaultCancelLabel() const;
    virtual wxString GetDefaultHelpLabel() const;

    // create the real GTK+ dialog: this is done from ShowModal() to allow
    // changing the message between constructing the dialog and showing it
    void GTKCreateMsgDialog();

    DECLARE_DYNAMIC_CLASS(wxMessageDialog)
};

#endif // _WX_GTK_MSGDLG_H_
