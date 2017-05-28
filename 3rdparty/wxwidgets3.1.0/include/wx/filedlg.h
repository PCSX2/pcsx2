/////////////////////////////////////////////////////////////////////////////
// Name:        wx/filedlg.h
// Purpose:     wxFileDialog base header
// Author:      Robert Roebling
// Modified by:
// Created:     8/17/99
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEDLG_H_BASE_
#define _WX_FILEDLG_H_BASE_

#include "wx/defs.h"

#if wxUSE_FILEDLG

#include "wx/dialog.h"
#include "wx/arrstr.h"

// this symbol is defined for the platforms which support multiple
// ('|'-separated) filters in the file dialog
#if defined(__WXMSW__) || defined(__WXGTK__) || defined(__WXMAC__)
    #define wxHAS_MULTIPLE_FILEDLG_FILTERS
#endif

//----------------------------------------------------------------------------
// wxFileDialog data
//----------------------------------------------------------------------------

/*
    The flags below must coexist with the following flags in m_windowStyle
    #define wxCAPTION               0x20000000
    #define wxMAXIMIZE              0x00002000
    #define wxCLOSE_BOX             0x00001000
    #define wxSYSTEM_MENU           0x00000800
    wxBORDER_NONE   =               0x00200000
    #define wxRESIZE_BORDER         0x00000040
    #define wxDIALOG_NO_PARENT      0x00000020
*/

enum
{
    wxFD_OPEN              = 0x0001,
    wxFD_SAVE              = 0x0002,
    wxFD_OVERWRITE_PROMPT  = 0x0004,
    wxFD_NO_FOLLOW         = 0x0008,
    wxFD_FILE_MUST_EXIST   = 0x0010,
    wxFD_CHANGE_DIR        = 0x0080,
    wxFD_PREVIEW           = 0x0100,
    wxFD_MULTIPLE          = 0x0200
};

#define wxFD_DEFAULT_STYLE      wxFD_OPEN

extern WXDLLIMPEXP_DATA_CORE(const char) wxFileDialogNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxFileSelectorPromptStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxFileSelectorDefaultWildcardStr[];

//----------------------------------------------------------------------------
// wxFileDialogBase
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDialogBase: public wxDialog
{
public:
    wxFileDialogBase () { Init(); }

    wxFileDialogBase(wxWindow *parent,
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
        Create(parent, message, defaultDir, defaultFile, wildCard, style, pos, sz, name);
    }

    virtual ~wxFileDialogBase() {}


    bool Create(wxWindow *parent,
                const wxString& message = wxFileSelectorPromptStr,
                const wxString& defaultDir = wxEmptyString,
                const wxString& defaultFile = wxEmptyString,
                const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                long style = wxFD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& sz = wxDefaultSize,
                const wxString& name = wxFileDialogNameStr);

    bool HasFdFlag(int flag) const { return HasFlag(flag); }

    virtual void SetMessage(const wxString& message) { m_message = message; }
    virtual void SetPath(const wxString& path);
    virtual void SetDirectory(const wxString& dir);
    virtual void SetFilename(const wxString& name);
    virtual void SetWildcard(const wxString& wildCard) { m_wildCard = wildCard; }
    virtual void SetFilterIndex(int filterIndex) { m_filterIndex = filterIndex; }

    virtual wxString GetMessage() const { return m_message; }
    virtual wxString GetPath() const { return m_path; }
    virtual void GetPaths(wxArrayString& paths) const { paths.Empty(); paths.Add(m_path); }
    virtual wxString GetDirectory() const { return m_dir; }
    virtual wxString GetFilename() const { return m_fileName; }
    virtual void GetFilenames(wxArrayString& files) const { files.Empty(); files.Add(m_fileName); }
    virtual wxString GetWildcard() const { return m_wildCard; }
    virtual int GetFilterIndex() const { return m_filterIndex; }

    virtual wxString GetCurrentlySelectedFilename() const
        { return m_currentlySelectedFilename; }

    // this function is called with wxFileDialog as parameter and should
    // create the window containing the extra controls we want to show in it
    typedef wxWindow *(*ExtraControlCreatorFunction)(wxWindow*);

    virtual bool SupportsExtraControl() const { return false; }

    bool SetExtraControlCreator(ExtraControlCreatorFunction creator);
    wxWindow *GetExtraControl() const { return m_extraControl; }

    // Utility functions

    // Append first extension to filePath from a ';' separated extensionList
    // if filePath = "path/foo.bar" just return it as is
    // if filePath = "foo[.]" and extensionList = "*.jpg;*.png" return "foo.jpg"
    // if the extension is "*.j?g" (has wildcards) or "jpg" then return filePath
    static wxString AppendExtension(const wxString &filePath,
                                    const wxString &extensionList);

    // Set the filter index to match the given extension.
    //
    // This is always valid to call, even if the extension is empty or the
    // filter list doesn't contain it, the function will just do nothing in
    // these cases.
    void SetFilterIndexFromExt(const wxString& ext);

protected:
    wxString      m_message;
    wxString      m_dir;
    wxString      m_path;       // Full path
    wxString      m_fileName;
    wxString      m_wildCard;
    int           m_filterIndex;

    // Currently selected, but not yet necessarily accepted by the user, file.
    // This should be updated whenever the selection in the control changes by
    // the platform-specific code to provide a useful implementation of
    // GetCurrentlySelectedFilename().
    wxString      m_currentlySelectedFilename;

    wxWindow*     m_extraControl;

    // returns true if control is created (if it already exists returns false)
    bool CreateExtraControl();
    // return true if SetExtraControlCreator() was called
    bool HasExtraControlCreator() const
        { return m_extraControlCreator != NULL; }
    // get the size of the extra control by creating and deleting it
    wxSize GetExtraControlSize();

private:
    ExtraControlCreatorFunction m_extraControlCreator;

    void Init();
    wxDECLARE_DYNAMIC_CLASS(wxFileDialogBase);
    wxDECLARE_NO_COPY_CLASS(wxFileDialogBase);
};


//----------------------------------------------------------------------------
// wxFileDialog convenience functions
//----------------------------------------------------------------------------

// File selector - backward compatibility
WXDLLIMPEXP_CORE wxString
wxFileSelector(const wxString& message = wxFileSelectorPromptStr,
               const wxString& default_path = wxEmptyString,
               const wxString& default_filename = wxEmptyString,
               const wxString& default_extension = wxEmptyString,
               const wxString& wildcard = wxFileSelectorDefaultWildcardStr,
               int flags = 0,
               wxWindow *parent = NULL,
               int x = wxDefaultCoord, int y = wxDefaultCoord);

// An extended version of wxFileSelector
WXDLLIMPEXP_CORE wxString
wxFileSelectorEx(const wxString& message = wxFileSelectorPromptStr,
                 const wxString& default_path = wxEmptyString,
                 const wxString& default_filename = wxEmptyString,
                 int *indexDefaultExtension = NULL,
                 const wxString& wildcard = wxFileSelectorDefaultWildcardStr,
                 int flags = 0,
                 wxWindow *parent = NULL,
                 int x = wxDefaultCoord, int y = wxDefaultCoord);

// Ask for filename to load
WXDLLIMPEXP_CORE wxString
wxLoadFileSelector(const wxString& what,
                   const wxString& extension,
                   const wxString& default_name = wxEmptyString,
                   wxWindow *parent = NULL);

// Ask for filename to save
WXDLLIMPEXP_CORE wxString
wxSaveFileSelector(const wxString& what,
                   const wxString& extension,
                   const wxString& default_name = wxEmptyString,
                   wxWindow *parent = NULL);


#if defined (__WXUNIVERSAL__)
    #define wxHAS_GENERIC_FILEDIALOG
    #include "wx/generic/filedlgg.h"
#elif defined(__WXMSW__)
    #include "wx/msw/filedlg.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/filedlg.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/filedlg.h"     // GTK+ > 2.4 has native version
#elif defined(__WXGTK__)
    #include "wx/gtk1/filedlg.h"
#elif defined(__WXMAC__)
    #include "wx/osx/filedlg.h"
#elif defined(__WXQT__)
    #include "wx/qt/filedlg.h"
#endif

#endif // wxUSE_FILEDLG

#endif // _WX_FILEDLG_H_BASE_
