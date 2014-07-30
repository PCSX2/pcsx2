/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/filedlg.h
// Purpose:     wxFileDialog class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEDLG_H_
#define _WX_FILEDLG_H_

class WXDLLIMPEXP_FWD_CORE wxChoice;

//-------------------------------------------------------------------------
// wxFileDialog
//-------------------------------------------------------------------------

// set this system option to 1 in order to always show the filetypes popup in 
// file open dialogs if possible

#define wxOSX_FILEDIALOG_ALWAYS_SHOW_TYPES wxT("osx.openfiledialog.always-show-types")

class WXDLLIMPEXP_CORE wxFileDialog: public wxFileDialogBase
{
DECLARE_DYNAMIC_CLASS(wxFileDialog)
protected:
    wxArrayString m_fileNames;
    wxArrayString m_paths;

public:
    wxFileDialog() { Init(); }
    wxFileDialog(wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFD_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& sz = wxDefaultSize,
                 const wxString& name = wxFileDialogNameStr)
    {
        Init();

        Create(parent,message,defaultDir,defaultFile,wildCard,style,pos,sz,name);
    }

    void Create(wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFD_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& sz = wxDefaultSize,
                 const wxString& name = wxFileDialogNameStr);

#if wxOSX_USE_COCOA
    ~wxFileDialog();
#endif
    
    virtual void GetPaths(wxArrayString& paths) const { paths = m_paths; }
    virtual void GetFilenames(wxArrayString& files) const { files = m_fileNames ; }

    virtual int ShowModal();

#if wxOSX_USE_COCOA
    virtual void ShowWindowModal();
    virtual void ModalFinishedCallback(void* panel, int resultCode);
#endif

    virtual bool SupportsExtraControl() const;
    
    // implementation only
    
#if wxOSX_USE_COCOA
    // returns true if the file can be shown as active
    bool CheckFile( const wxString& filename );
#endif

protected:
    // not supported for file dialog, RR
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}

    void SetupExtraControls(WXWindow nativeWindow);
    
#if wxOSX_USE_COCOA
    virtual wxWindow* CreateFilterPanel(wxWindow *extracontrol);
    void DoOnFilterSelected(int index);
    virtual void OnFilterSelected(wxCommandEvent &event);

    wxArrayString m_filterExtensions;
    wxArrayString m_filterNames;
    wxChoice* m_filterChoice;
    wxWindow* m_filterPanel;
    bool m_useFileTypeFilter;
    int m_firstFileTypeFilter;
    wxArrayString m_currentExtensions;
    WX_NSObject m_delegate;
    WX_NSObject m_sheetDelegate;
#endif

private:
    // Common part of all ctors.
    void Init();
};

#endif // _WX_FILEDLG_H_
