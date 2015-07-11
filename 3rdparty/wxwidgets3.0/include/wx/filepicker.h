/////////////////////////////////////////////////////////////////////////////
// Name:        wx/filepicker.h
// Purpose:     wxFilePickerCtrl, wxDirPickerCtrl base header
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEDIRPICKER_H_BASE_
#define _WX_FILEDIRPICKER_H_BASE_

#include "wx/defs.h"

#if wxUSE_FILEPICKERCTRL || wxUSE_DIRPICKERCTRL

#include "wx/pickerbase.h"
#include "wx/filename.h"

class WXDLLIMPEXP_FWD_CORE wxDialog;
class WXDLLIMPEXP_FWD_CORE wxFileDirPickerEvent;

extern WXDLLIMPEXP_DATA_CORE(const char) wxFilePickerWidgetLabel[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxFilePickerWidgetNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxFilePickerCtrlNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxFileSelectorPromptStr[];

extern WXDLLIMPEXP_DATA_CORE(const char) wxDirPickerWidgetLabel[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxDirPickerWidgetNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxDirPickerCtrlNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxDirSelectorPromptStr[];

// ----------------------------------------------------------------------------
// wxFileDirPickerEvent: used by wxFilePickerCtrl and wxDirPickerCtrl only
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDirPickerEvent : public wxCommandEvent
{
public:
    wxFileDirPickerEvent() {}
    wxFileDirPickerEvent(wxEventType type, wxObject *generator, int id, const wxString &path)
        : wxCommandEvent(type, id),
          m_path(path)
    {
        SetEventObject(generator);
    }

    wxString GetPath() const { return m_path; }
    void SetPath(const wxString &p) { m_path = p; }

    // default copy ctor, assignment operator and dtor are ok
    virtual wxEvent *Clone() const { return new wxFileDirPickerEvent(*this); }

private:
    wxString m_path;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxFileDirPickerEvent)
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_FILEPICKER_CHANGED, wxFileDirPickerEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_DIRPICKER_CHANGED, wxFileDirPickerEvent );

// ----------------------------------------------------------------------------
// event types and macros
// ----------------------------------------------------------------------------

typedef void (wxEvtHandler::*wxFileDirPickerEventFunction)(wxFileDirPickerEvent&);

#define wxFileDirPickerEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxFileDirPickerEventFunction, func)

#define EVT_FILEPICKER_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_FILEPICKER_CHANGED, id, wxFileDirPickerEventHandler(fn))
#define EVT_DIRPICKER_CHANGED(id, fn) \
    wx__DECLARE_EVT1(wxEVT_DIRPICKER_CHANGED, id, wxFileDirPickerEventHandler(fn))

// ----------------------------------------------------------------------------
// wxFileDirPickerWidgetBase: a generic abstract interface which must be
//                           implemented by controls used by wxFileDirPickerCtrlBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDirPickerWidgetBase
{
public:
    wxFileDirPickerWidgetBase() {  }
    virtual ~wxFileDirPickerWidgetBase() {  }

    // Path here is the name of the selected file or directory.
    wxString GetPath() const { return m_path; }
    virtual void SetPath(const wxString &str) { m_path=str; }

    // Set the directory to open the file browse dialog at initially.
    virtual void SetInitialDirectory(const wxString& dir) = 0;

    // returns the picker widget cast to wxControl
    virtual wxControl *AsControl() = 0;

protected:
    virtual void UpdateDialogPath(wxDialog *) = 0;
    virtual void UpdatePathFromDialog(wxDialog *) = 0;

    wxString m_path;
};

// Styles which must be supported by all controls implementing wxFileDirPickerWidgetBase
// NB: these styles must be defined to carefully-chosen values to
//     avoid conflicts with wxButton's styles

#define wxFLP_OPEN                    0x0400
#define wxFLP_SAVE                    0x0800
#define wxFLP_OVERWRITE_PROMPT        0x1000
#define wxFLP_FILE_MUST_EXIST         0x2000
#define wxFLP_CHANGE_DIR              0x4000
#define wxFLP_SMALL                   wxPB_SMALL

// NOTE: wxMULTIPLE is not supported !


#define wxDIRP_DIR_MUST_EXIST         0x0008
#define wxDIRP_CHANGE_DIR             0x0010
#define wxDIRP_SMALL                  wxPB_SMALL


// map platform-dependent controls which implement the wxFileDirPickerWidgetBase
// under the name "wxFilePickerWidget" and "wxDirPickerWidget".
// NOTE: wxFileDirPickerCtrlBase will allocate a wx{File|Dir}PickerWidget and this
//       requires that all classes being mapped as wx{File|Dir}PickerWidget have the
//       same prototype for the contructor...
// since GTK >= 2.6, there is GtkFileButton
#if defined(__WXGTK20__) && !defined(__WXUNIVERSAL__)
    #include "wx/gtk/filepicker.h"
    #define wxFilePickerWidget      wxFileButton
    #define wxDirPickerWidget       wxDirButton
#else
    #include "wx/generic/filepickerg.h"
    #define wxFilePickerWidget      wxGenericFileButton
    #define wxDirPickerWidget       wxGenericDirButton
#endif



// ----------------------------------------------------------------------------
// wxFileDirPickerCtrlBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDirPickerCtrlBase : public wxPickerBase
{
public:
    wxFileDirPickerCtrlBase() {}

protected:
    // NB: no default values since this function will never be used
    //     directly by the user and derived classes wouldn't use them
    bool CreateBase(wxWindow *parent,
                    wxWindowID id,
                    const wxString& path,
                    const wxString &message,
                    const wxString &wildcard,
                    const wxPoint& pos,
                    const wxSize& size,
                    long style,
                    const wxValidator& validator,
                    const wxString& name);

public:         // public API

    wxString GetPath() const;
    void SetPath(const wxString &str);

    // Set the directory to open the file browse dialog at initially.
    void SetInitialDirectory(const wxString& dir)
    {
        m_pickerIface->SetInitialDirectory(dir);
    }

public:        // internal functions

    void UpdatePickerFromTextCtrl();
    void UpdateTextCtrlFromPicker();

    // event handler for our picker
    void OnFileDirChange(wxFileDirPickerEvent &);

    // TRUE if any textctrl change should update the current working directory
    virtual bool IsCwdToUpdate() const = 0;

    // Returns the event type sent by this picker
    virtual wxEventType GetEventType() const = 0;

    virtual void DoConnect( wxControl *sender, wxFileDirPickerCtrlBase *eventSink ) = 0;

    // Returns the filtered value currently placed in the text control (if present).
    virtual wxString GetTextCtrlValue() const = 0;

protected:
    // creates the picker control
    virtual
    wxFileDirPickerWidgetBase *CreatePicker(wxWindow *parent,
                                            const wxString& path,
                                            const wxString& message,
                                            const wxString& wildcard) = 0;

protected:

    // m_picker object as wxFileDirPickerWidgetBase interface
    wxFileDirPickerWidgetBase *m_pickerIface;
};

#endif  // wxUSE_FILEPICKERCTRL || wxUSE_DIRPICKERCTRL


#if wxUSE_FILEPICKERCTRL

// ----------------------------------------------------------------------------
// wxFilePickerCtrl: platform-independent class which embeds the
// platform-dependent wxFilePickerWidget and, if wxFLP_USE_TEXTCTRL style is
// used, a textctrl next to it.
// ----------------------------------------------------------------------------

#define wxFLP_USE_TEXTCTRL            (wxPB_USE_TEXTCTRL)

#ifdef __WXGTK__
    // GTK apps usually don't have a textctrl next to the picker
    #define wxFLP_DEFAULT_STYLE       (wxFLP_OPEN|wxFLP_FILE_MUST_EXIST)
#else
    #define wxFLP_DEFAULT_STYLE       (wxFLP_USE_TEXTCTRL|wxFLP_OPEN|wxFLP_FILE_MUST_EXIST)
#endif

class WXDLLIMPEXP_CORE wxFilePickerCtrl : public wxFileDirPickerCtrlBase
{
public:
    wxFilePickerCtrl() {}

    wxFilePickerCtrl(wxWindow *parent,
                     wxWindowID id,
                     const wxString& path = wxEmptyString,
                     const wxString& message = wxFileSelectorPromptStr,
                     const wxString& wildcard = wxFileSelectorDefaultWildcardStr,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxFLP_DEFAULT_STYLE,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxFilePickerCtrlNameStr)
    {
        Create(parent, id, path, message, wildcard, pos, size, style,
               validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& path = wxEmptyString,
                const wxString& message = wxFileSelectorPromptStr,
                const wxString& wildcard = wxFileSelectorDefaultWildcardStr,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxFLP_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxFilePickerCtrlNameStr);

    void SetFileName(const wxFileName &filename)
        { SetPath(filename.GetFullPath()); }

    wxFileName GetFileName() const
        { return wxFileName(GetPath()); }

public:     // overrides

    // return the text control value in canonical form
    wxString GetTextCtrlValue() const;

    bool IsCwdToUpdate() const
        { return HasFlag(wxFLP_CHANGE_DIR); }

    wxEventType GetEventType() const
        { return wxEVT_FILEPICKER_CHANGED; }

    virtual void DoConnect( wxControl *sender, wxFileDirPickerCtrlBase *eventSink )
    {
        sender->Connect( wxEVT_FILEPICKER_CHANGED,
            wxFileDirPickerEventHandler( wxFileDirPickerCtrlBase::OnFileDirChange ),
            NULL, eventSink );
    }


protected:
    virtual
    wxFileDirPickerWidgetBase *CreatePicker(wxWindow *parent,
                                            const wxString& path,
                                            const wxString& message,
                                            const wxString& wildcard)
    {
        return new wxFilePickerWidget(parent, wxID_ANY,
                                      wxGetTranslation(wxFilePickerWidgetLabel),
                                      path, message, wildcard,
                                      wxDefaultPosition, wxDefaultSize,
                                      GetPickerStyle(GetWindowStyle()));
    }

    // extracts the style for our picker from wxFileDirPickerCtrlBase's style
    long GetPickerStyle(long style) const
    {
        return style & (wxFLP_OPEN |
                        wxFLP_SAVE |
                        wxFLP_OVERWRITE_PROMPT |
                        wxFLP_FILE_MUST_EXIST |
                        wxFLP_CHANGE_DIR |
                        wxFLP_USE_TEXTCTRL |
                        wxFLP_SMALL);
    }

private:
    DECLARE_DYNAMIC_CLASS(wxFilePickerCtrl)
};

#endif      // wxUSE_FILEPICKERCTRL


#if wxUSE_DIRPICKERCTRL

// ----------------------------------------------------------------------------
// wxDirPickerCtrl: platform-independent class which embeds the
// platform-dependent wxDirPickerWidget and eventually a textctrl
// (see wxDIRP_USE_TEXTCTRL) next to it.
// ----------------------------------------------------------------------------

#define wxDIRP_USE_TEXTCTRL            (wxPB_USE_TEXTCTRL)

#ifdef __WXGTK__
    // GTK apps usually don't have a textctrl next to the picker
    #define wxDIRP_DEFAULT_STYLE       (wxDIRP_DIR_MUST_EXIST)
#else
    #define wxDIRP_DEFAULT_STYLE       (wxDIRP_USE_TEXTCTRL|wxDIRP_DIR_MUST_EXIST)
#endif

class WXDLLIMPEXP_CORE wxDirPickerCtrl : public wxFileDirPickerCtrlBase
{
public:
    wxDirPickerCtrl() {}

    wxDirPickerCtrl(wxWindow *parent, wxWindowID id,
                    const wxString& path = wxEmptyString,
                    const wxString& message = wxDirSelectorPromptStr,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxDIRP_DEFAULT_STYLE,
                    const wxValidator& validator = wxDefaultValidator,
                    const wxString& name = wxDirPickerCtrlNameStr)
    {
        Create(parent, id, path, message, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& path = wxEmptyString,
                const wxString& message = wxDirSelectorPromptStr,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDIRP_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxDirPickerCtrlNameStr);

    void SetDirName(const wxFileName &dirname)
        { SetPath(dirname.GetPath()); }

    wxFileName GetDirName() const
        { return wxFileName::DirName(GetPath()); }

public:     // overrides

    wxString GetTextCtrlValue() const;

    bool IsCwdToUpdate() const
        { return HasFlag(wxDIRP_CHANGE_DIR); }

    wxEventType GetEventType() const
        { return wxEVT_DIRPICKER_CHANGED; }

    virtual void DoConnect( wxControl *sender, wxFileDirPickerCtrlBase *eventSink )
    {
        sender->Connect( wxEVT_DIRPICKER_CHANGED,
            wxFileDirPickerEventHandler( wxFileDirPickerCtrlBase::OnFileDirChange ),
            NULL, eventSink );
    }


protected:
    virtual
    wxFileDirPickerWidgetBase *CreatePicker(wxWindow *parent,
                                            const wxString& path,
                                            const wxString& message,
                                            const wxString& WXUNUSED(wildcard))
    {
        return new wxDirPickerWidget(parent, wxID_ANY,
                                     wxGetTranslation(wxDirPickerWidgetLabel),
                                     path, message,
                                     wxDefaultPosition, wxDefaultSize,
                                     GetPickerStyle(GetWindowStyle()));
    }

    // extracts the style for our picker from wxFileDirPickerCtrlBase's style
    long GetPickerStyle(long style) const
    {
        return style & (wxDIRP_DIR_MUST_EXIST |
                        wxDIRP_CHANGE_DIR |
                        wxDIRP_USE_TEXTCTRL |
                        wxDIRP_SMALL);
    }

private:
    DECLARE_DYNAMIC_CLASS(wxDirPickerCtrl)
};

#endif      // wxUSE_DIRPICKERCTRL

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_FILEPICKER_CHANGED   wxEVT_FILEPICKER_CHANGED
#define wxEVT_COMMAND_DIRPICKER_CHANGED    wxEVT_DIRPICKER_CHANGED

#endif // _WX_FILEDIRPICKER_H_BASE_

