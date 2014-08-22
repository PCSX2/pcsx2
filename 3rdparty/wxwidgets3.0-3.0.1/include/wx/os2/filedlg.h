/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/filedlg.h
// Purpose:     wxFileDialog class
// Author:      David Webster
// Modified by:
// Created:     10/05/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEDLG_H_
#define _WX_FILEDLG_H_

//-------------------------------------------------------------------------
// wxFileDialog
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDialog: public wxFileDialogBase
{
DECLARE_DYNAMIC_CLASS(wxFileDialog)
public:
    wxFileDialog( wxWindow*       pParent
                 ,const wxString& rsMessage = wxFileSelectorPromptStr
                 ,const wxString& rsDefaultDir = wxEmptyString
                 ,const wxString& rsDefaultFile = wxEmptyString
                 ,const wxString& rsWildCard = wxFileSelectorDefaultWildcardStr
                 ,long            lStyle = wxFD_DEFAULT_STYLE
                 ,const wxPoint&  rPos = wxDefaultPosition,
                  const wxSize& sz = wxDefaultSize,
                  const wxString& name = wxFileDialogNameStr
                );

    virtual void GetPaths(wxArrayString& rasPath) const;

    int ShowModal();

protected:
    wxArrayString m_fileNames;
}; // end of CLASS wxFileDialog

#endif // _WX_FILEDLG_H_
