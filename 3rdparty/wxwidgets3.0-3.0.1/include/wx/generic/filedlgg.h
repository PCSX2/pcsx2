/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/filedlgg.h
// Purpose:     wxGenericFileDialog
// Author:      Robert Roebling
// Modified by:
// Created:     8/17/99
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEDLGG_H_
#define _WX_FILEDLGG_H_

#include "wx/listctrl.h"
#include "wx/datetime.h"
#include "wx/filefn.h"
#include "wx/artprov.h"
#include "wx/filedlg.h"
#include "wx/generic/filectrlg.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxBitmapButton;
class WXDLLIMPEXP_FWD_CORE wxGenericFileCtrl;
class WXDLLIMPEXP_FWD_CORE wxGenericFileDialog;
class WXDLLIMPEXP_FWD_CORE wxFileCtrlEvent;

//-------------------------------------------------------------------------
// wxGenericFileDialog
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericFileDialog: public wxFileDialogBase
{
public:
    wxGenericFileDialog() : wxFileDialogBase() { Init(); }

    wxGenericFileDialog(wxWindow *parent,
                        const wxString& message = wxFileSelectorPromptStr,
                        const wxString& defaultDir = wxEmptyString,
                        const wxString& defaultFile = wxEmptyString,
                        const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                        long style = wxFD_DEFAULT_STYLE,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& sz = wxDefaultSize,
                        const wxString& name = wxFileDialogNameStr,
                        bool bypassGenericImpl = false );

    bool Create( wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFD_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& sz = wxDefaultSize,
                 const wxString& name = wxFileDialogNameStr,
                 bool bypassGenericImpl = false );

    virtual ~wxGenericFileDialog();

    virtual void SetDirectory(const wxString& dir)
        { m_filectrl->SetDirectory(dir); }
    virtual void SetFilename(const wxString& name)
        { m_filectrl->SetFilename(name); }
    virtual void SetMessage(const wxString& message) { SetTitle(message); }
    virtual void SetPath(const wxString& path)
        { m_filectrl->SetPath(path); }
    virtual void SetFilterIndex(int filterIndex)
        { m_filectrl->SetFilterIndex(filterIndex); }
    virtual void SetWildcard(const wxString& wildCard)
        { m_filectrl->SetWildcard(wildCard); }

    virtual wxString GetPath() const
        { return m_filectrl->GetPath(); }
    virtual void GetPaths(wxArrayString& paths) const
        { m_filectrl->GetPaths(paths); }
    virtual wxString GetDirectory() const
        { return m_filectrl->GetDirectory(); }
    virtual wxString GetFilename() const
        { return m_filectrl->GetFilename(); }
    virtual void GetFilenames(wxArrayString& files) const
        { m_filectrl->GetFilenames(files); }
    virtual wxString GetWildcard() const
        { return m_filectrl->GetWildcard(); }
    virtual int GetFilterIndex() const
        { return m_filectrl->GetFilterIndex(); }
    virtual bool SupportsExtraControl() const { return true; }

    // implementation only from now on
    // -------------------------------

    virtual int ShowModal();
    virtual bool Show( bool show = true );

    void OnList( wxCommandEvent &event );
    void OnReport( wxCommandEvent &event );
    void OnUp( wxCommandEvent &event );
    void OnHome( wxCommandEvent &event );
    void OnOk( wxCommandEvent &event );
    void OnNew( wxCommandEvent &event );
    void OnFileActivated( wxFileCtrlEvent &event);

private:
    // if true, don't use this implementation at all
    bool m_bypassGenericImpl;

protected:
    // update the state of m_upDirButton and m_newDirButton depending on the
    // currently selected directory
    void OnUpdateButtonsUI(wxUpdateUIEvent& event);

    wxString               m_filterExtension;
    wxGenericFileCtrl     *m_filectrl;
    wxBitmapButton        *m_upDirButton;
    wxBitmapButton        *m_newDirButton;

private:
    void Init();
    wxBitmapButton* AddBitmapButton( wxWindowID winId, const wxArtID& artId,
                                     const wxString& tip, wxSizer *sizer );

    DECLARE_DYNAMIC_CLASS(wxGenericFileDialog)
    DECLARE_EVENT_TABLE()

    // these variables are preserved between wxGenericFileDialog calls
    static long ms_lastViewStyle;     // list or report?
    static bool ms_lastShowHidden;    // did we show hidden files?
};

#ifdef wxHAS_GENERIC_FILEDIALOG

class WXDLLIMPEXP_CORE wxFileDialog: public wxGenericFileDialog
{
public:
    wxFileDialog() {}

    wxFileDialog(wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = 0,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize)
          :wxGenericFileDialog(parent, message,
                               defaultDir, defaultFile, wildCard,
                               style,
                               pos, size)
     {
     }

private:
     DECLARE_DYNAMIC_CLASS(wxFileDialog)
};

#endif // wxHAS_GENERIC_FILEDIALOG

#endif // _WX_FILEDLGG_H_
