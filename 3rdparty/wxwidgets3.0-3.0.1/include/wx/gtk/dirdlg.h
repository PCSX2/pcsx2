/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dirdlg.h
// Purpose:     wxDirDialog
// Author:      Francesco Montorsi
// Copyright:   (c) 2006 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKDIRDLGH__
#define __GTKDIRDLGH__

//-------------------------------------------------------------------------
// wxDirDialog
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDirDialog : public wxDirDialogBase
{
public:
    wxDirDialog() { }

    wxDirDialog(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxEmptyString,
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr);
    bool Create(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxEmptyString,
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr);
    virtual ~wxDirDialog() { }


public:     // overrides from wxGenericDirDialog

    wxString GetPath() const;
    void SetPath(const wxString& path);


    // Implementation only.

    void GTKOnAccept();
    void GTKOnCancel();

protected:
    // override this from wxTLW since the native
    // form doesn't have any m_wxwindow
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);


private:
    wxString m_selectedDirectory;

    DECLARE_DYNAMIC_CLASS(wxDirDialog)
};

#endif // __GTKDIRDLGH__
