/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/filedlg.h
// Purpose:     wxFileDialog class
// Author:      Ryan Norton
// Modified by:
// Created:     2004-10-02
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_FILEDLG_H_
#define _WX_COCOA_FILEDLG_H_

DECLARE_WXCOCOA_OBJC_CLASS(NSSavePanel);

#define wxFileDialog wxCocoaFileDialog
//-------------------------------------------------------------------------
// wxFileDialog
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDialog: public wxFileDialogBase
{
    DECLARE_DYNAMIC_CLASS(wxFileDialog)
    wxDECLARE_NO_COPY_CLASS(wxFileDialog);
public:
    wxFileDialog(wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFD_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& sz = wxDefaultSize,
                 const wxString& name = wxFileDialogNameStr);
    virtual ~wxFileDialog();

    virtual void SetPath(const wxString& path);
    virtual void GetPaths(wxArrayString& paths) const;
    virtual void GetFilenames(wxArrayString& files) const;

    virtual int ShowModal();

    inline WX_NSSavePanel GetNSSavePanel()
    {   return (WX_NSSavePanel)m_cocoaNSWindow; }

private:
    WX_NSMutableArray m_wildcards;
    wxArrayString m_fileNames;
};

#endif // _WX_FILEDLG_H_

