/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dialog.h
// Purpose:
// Author:      Robert Roebling
// Created:
// Copyright:   (c) 1998 Robert Roebling
// Licence:           wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDIALOG_H_
#define _WX_GTKDIALOG_H_

class WXDLLIMPEXP_FWD_CORE wxGUIEventLoop;

//-----------------------------------------------------------------------------
// wxDialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDialog: public wxDialogBase
{
public:
    wxDialog() { Init(); }
    wxDialog( wxWindow *parent, wxWindowID id,
            const wxString &title,
            const wxPoint &pos = wxDefaultPosition,
            const wxSize &size = wxDefaultSize,
            long style = wxDEFAULT_DIALOG_STYLE,
            const wxString &name = wxDialogNameStr );
    bool Create( wxWindow *parent, wxWindowID id,
            const wxString &title,
            const wxPoint &pos = wxDefaultPosition,
            const wxSize &size = wxDefaultSize,
            long style = wxDEFAULT_DIALOG_STYLE,
            const wxString &name = wxDialogNameStr );
    virtual ~wxDialog();

    virtual bool Show( bool show = true );
    virtual int ShowModal();
    virtual void EndModal( int retCode );
    virtual bool IsModal() const;

private:
    // common part of all ctors
    void Init();

    bool m_modalShowing;
    wxGUIEventLoop *m_modalLoop;

    DECLARE_DYNAMIC_CLASS(wxDialog)
};

#endif // _WX_GTKDIALOG_H_
