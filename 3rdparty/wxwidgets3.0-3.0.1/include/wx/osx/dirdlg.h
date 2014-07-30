/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dirdlg.h
// Purpose:     wxDirDialog class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIRDLG_H_
#define _WX_DIRDLG_H_

#if wxOSX_USE_COCOA
    DECLARE_WXCOCOA_OBJC_CLASS(NSOpenPanel);
#endif

class WXDLLIMPEXP_CORE wxDirDialog : public wxDirDialogBase
{
public:
    wxDirDialog() { Init(); }

    wxDirDialog(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxT(""),
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr)
    {
        Init();

        Create(parent,message,defaultPath,style,pos,size,name);
    }

    void Create(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxT(""),
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr);

#if wxOSX_USE_COCOA
    ~wxDirDialog();
#endif

    virtual int ShowModal();

#if wxOSX_USE_COCOA
    virtual void ShowWindowModal();
    virtual void ModalFinishedCallback(void* panel, int returnCode);
#endif

private:
#if wxOSX_USE_COCOA
    // Create and initialize NSOpenPanel that we use in both ShowModal() and
    // ShowWindowModal().
    WX_NSOpenPanel OSXCreatePanel() const;

    WX_NSObject m_sheetDelegate;
#endif

    // Common part of all ctors.
    void Init();

    DECLARE_DYNAMIC_CLASS(wxDirDialog)
};

#endif // _WX_DIRDLG_H_
