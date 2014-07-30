/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/filepickerg.h
// Purpose:     wxGenericFileDirButton, wxGenericFileButton, wxGenericDirButton
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEDIRPICKER_H_
#define _WX_FILEDIRPICKER_H_

#include "wx/button.h"
#include "wx/filedlg.h"
#include "wx/dirdlg.h"


wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_DIRPICKER_CHANGED, wxFileDirPickerEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FILEPICKER_CHANGED, wxFileDirPickerEvent );


//-----------------------------------------------------------------------------
// wxGenericFileDirButton: a button which brings up a wx{File|Dir}Dialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericFileDirButton : public wxButton,
                                                public wxFileDirPickerWidgetBase
{
public:
    wxGenericFileDirButton() { Init(); }
    wxGenericFileDirButton(wxWindow *parent,
                           wxWindowID id,
                           const wxString& label = wxFilePickerWidgetLabel,
                           const wxString& path = wxEmptyString,
                           const wxString &message = wxFileSelectorPromptStr,
                           const wxString &wildcard = wxFileSelectorDefaultWildcardStr,
                           const wxPoint& pos = wxDefaultPosition,
                           const wxSize& size = wxDefaultSize,
                           long style = 0,
                           const wxValidator& validator = wxDefaultValidator,
                           const wxString& name = wxFilePickerWidgetNameStr)
    {
        Init();
        Create(parent, id, label, path, message, wildcard,
               pos, size, style, validator, name);
    }

    virtual wxControl *AsControl() { return this; }

public:     // overridable

    virtual wxDialog *CreateDialog() = 0;

    virtual wxWindow *GetDialogParent()
        { return GetParent(); }

    virtual wxEventType GetEventType() const = 0;

    virtual void SetInitialDirectory(const wxString& dir);

public:

    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& label = wxFilePickerWidgetLabel,
           const wxString& path = wxEmptyString,
           const wxString &message = wxFileSelectorPromptStr,
           const wxString &wildcard = wxFileSelectorDefaultWildcardStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxFilePickerWidgetNameStr);

    // event handler for the click
    void OnButtonClick(wxCommandEvent &);

protected:
    wxString m_message, m_wildcard;

    // we just store the style passed to the ctor here instead of passing it to
    // wxButton as some of our bits can conflict with wxButton styles and it
    // just doesn't make sense to use picker styles for wxButton anyhow
    long m_pickerStyle;

    // Initial directory set by SetInitialDirectory() call or empty.
    wxString m_initialDir;

private:
    // common part of all ctors
    void Init() { m_pickerStyle = -1; }
};


//-----------------------------------------------------------------------------
// wxGenericFileButton: a button which brings up a wxFileDialog
//-----------------------------------------------------------------------------

#define wxFILEBTN_DEFAULT_STYLE                     (wxFLP_OPEN)

class WXDLLIMPEXP_CORE wxGenericFileButton : public wxGenericFileDirButton
{
public:
    wxGenericFileButton() {}
    wxGenericFileButton(wxWindow *parent,
                        wxWindowID id,
                        const wxString& label = wxFilePickerWidgetLabel,
                        const wxString& path = wxEmptyString,
                        const wxString &message = wxFileSelectorPromptStr,
                        const wxString &wildcard = wxFileSelectorDefaultWildcardStr,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxFILEBTN_DEFAULT_STYLE,
                        const wxValidator& validator = wxDefaultValidator,
                        const wxString& name = wxFilePickerWidgetNameStr)
    {
        Create(parent, id, label, path, message, wildcard,
               pos, size, style, validator, name);
    }

public:     // overridable

    virtual long GetDialogStyle() const
    {
        // the derived class must initialize it if it doesn't use the
        // non-default wxGenericFileDirButton ctor
        wxASSERT_MSG( m_pickerStyle != -1,
                      "forgot to initialize m_pickerStyle?" );


        long filedlgstyle = 0;

        if ( m_pickerStyle & wxFLP_OPEN )
            filedlgstyle |= wxFD_OPEN;
        if ( m_pickerStyle & wxFLP_SAVE )
            filedlgstyle |= wxFD_SAVE;
        if ( m_pickerStyle & wxFLP_OVERWRITE_PROMPT )
            filedlgstyle |= wxFD_OVERWRITE_PROMPT;
        if ( m_pickerStyle & wxFLP_FILE_MUST_EXIST )
            filedlgstyle |= wxFD_FILE_MUST_EXIST;
        if ( m_pickerStyle & wxFLP_CHANGE_DIR )
            filedlgstyle |= wxFD_CHANGE_DIR;

        return filedlgstyle;
    }

    virtual wxDialog *CreateDialog();

    wxEventType GetEventType() const
        { return wxEVT_FILEPICKER_CHANGED; }

protected:
    void UpdateDialogPath(wxDialog *p)
        { wxStaticCast(p, wxFileDialog)->SetPath(m_path); }
    void UpdatePathFromDialog(wxDialog *p)
        { m_path = wxStaticCast(p, wxFileDialog)->GetPath(); }

private:
    DECLARE_DYNAMIC_CLASS(wxGenericFileButton)
};


//-----------------------------------------------------------------------------
// wxGenericDirButton: a button which brings up a wxDirDialog
//-----------------------------------------------------------------------------

#define wxDIRBTN_DEFAULT_STYLE                     0

class WXDLLIMPEXP_CORE wxGenericDirButton : public wxGenericFileDirButton
{
public:
    wxGenericDirButton() {}
    wxGenericDirButton(wxWindow *parent,
                       wxWindowID id,
                       const wxString& label = wxDirPickerWidgetLabel,
                       const wxString& path = wxEmptyString,
                       const wxString &message = wxDirSelectorPromptStr,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& size = wxDefaultSize,
                       long style = wxDIRBTN_DEFAULT_STYLE,
                       const wxValidator& validator = wxDefaultValidator,
                       const wxString& name = wxDirPickerWidgetNameStr)
    {
        Create(parent, id, label, path, message, wxEmptyString,
               pos, size, style, validator, name);
    }

public:     // overridable

    virtual long GetDialogStyle() const
    {
        long dirdlgstyle = wxDD_DEFAULT_STYLE;

        if ( m_pickerStyle & wxDIRP_DIR_MUST_EXIST )
            dirdlgstyle |= wxDD_DIR_MUST_EXIST;
        if ( m_pickerStyle & wxDIRP_CHANGE_DIR )
            dirdlgstyle |= wxDD_CHANGE_DIR;

        return dirdlgstyle;
    }

    virtual wxDialog *CreateDialog();

    wxEventType GetEventType() const
        { return wxEVT_DIRPICKER_CHANGED; }

protected:
    void UpdateDialogPath(wxDialog *p)
        { wxStaticCast(p, wxDirDialog)->SetPath(m_path); }
    void UpdatePathFromDialog(wxDialog *p)
        { m_path = wxStaticCast(p, wxDirDialog)->GetPath(); }

private:
    DECLARE_DYNAMIC_CLASS(wxGenericDirButton)
};

// old wxEVT_COMMAND_* constants
//#define wxEVT_COMMAND_DIRPICKER_CHANGED wxEVT_DIRPICKER_CHANGED
//#define wxEVT_COMMAND_FILEPICKER_CHANGED wxEVT_FILEPICKER_CHANGED

#endif // _WX_FILEDIRPICKER_H_
