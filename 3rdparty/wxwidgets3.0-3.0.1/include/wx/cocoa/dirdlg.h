/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/dirdlg.h
// Purpose:     wxDirDialog class
// Author:      Ryan Norton
// Modified by: Hiroyuki Nakamura(maloninc)
// Created:     2006-01-10
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_DIRDLG_H_
#define _WX_COCOA_DIRDLG_H_

DECLARE_WXCOCOA_OBJC_CLASS(NSSavePanel);

#define wxDirDialog wxCocoaDirDialog
//-------------------------------------------------------------------------
// wxDirDialog
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDirDialog: public wxDirDialogBase
{
    DECLARE_DYNAMIC_CLASS(wxDirDialog)
    wxDECLARE_NO_COPY_CLASS(wxDirDialog);
public:
    wxDirDialog(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxT(""),
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr);
    virtual ~wxDirDialog();

    virtual int ShowModal();

    inline WX_NSSavePanel GetNSSavePanel()
    {   return (WX_NSSavePanel)m_cocoaNSWindow; }

protected:
    wxString    m_dir;
    wxWindow *  m_parent;
    wxString    m_fileName;

private:
    wxArrayString m_fileNames;
};

#endif // _WX_DIRDLG_H_

