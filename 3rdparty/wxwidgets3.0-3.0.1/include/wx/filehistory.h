/////////////////////////////////////////////////////////////////////////////
// Name:        wx/filehistory.h
// Purpose:     wxFileHistory class
// Author:      Julian Smart, Vaclav Slavik
// Created:     2010-05-03
// Copyright:   (c) Julian Smart, Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FILEHISTORY_H_
#define _WX_FILEHISTORY_H_

#include "wx/defs.h"

#if wxUSE_FILE_HISTORY

#include "wx/windowid.h"
#include "wx/object.h"
#include "wx/list.h"
#include "wx/string.h"
#include "wx/arrstr.h"

class WXDLLIMPEXP_FWD_CORE wxMenu;
class WXDLLIMPEXP_FWD_BASE wxConfigBase;
class WXDLLIMPEXP_FWD_BASE wxFileName;

// ----------------------------------------------------------------------------
// File history management
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileHistoryBase : public wxObject
{
public:
    wxFileHistoryBase(size_t maxFiles = 9, wxWindowID idBase = wxID_FILE1);

    // Operations
    virtual void AddFileToHistory(const wxString& file);
    virtual void RemoveFileFromHistory(size_t i);
    virtual int GetMaxFiles() const { return (int)m_fileMaxFiles; }
    virtual void UseMenu(wxMenu *menu);

    // Remove menu from the list (MDI child may be closing)
    virtual void RemoveMenu(wxMenu *menu);

#if wxUSE_CONFIG
    virtual void Load(const wxConfigBase& config);
    virtual void Save(wxConfigBase& config);
#endif // wxUSE_CONFIG

    virtual void AddFilesToMenu();
    virtual void AddFilesToMenu(wxMenu* menu); // Single menu

    // Accessors
    virtual wxString GetHistoryFile(size_t i) const { return m_fileHistory[i]; }
    virtual size_t GetCount() const { return m_fileHistory.GetCount(); }

    const wxList& GetMenus() const { return m_fileMenus; }

    // Set/get base id
    void SetBaseId(wxWindowID baseId) { m_idBase = baseId; }
    wxWindowID GetBaseId() const { return m_idBase; }

#if WXWIN_COMPATIBILITY_2_6
    // deprecated, use GetCount() instead
    wxDEPRECATED( size_t GetNoHistoryFiles() const );
#endif // WXWIN_COMPATIBILITY_2_6

protected:
    // Last n files
    wxArrayString     m_fileHistory;

    // Menus to maintain (may need several for an MDI app)
    wxList            m_fileMenus;

    // Max files to maintain
    size_t            m_fileMaxFiles;

private:
    // The ID of the first history menu item (Doesn't have to be wxID_FILE1)
    wxWindowID m_idBase;

    // Normalize a file name to canonical form. We have a special function for
    // this to ensure the same normalization is used everywhere.
    static wxString NormalizeFileName(const wxFileName& filename);

    wxDECLARE_NO_COPY_CLASS(wxFileHistoryBase);
};

#if WXWIN_COMPATIBILITY_2_6
inline size_t wxFileHistoryBase::GetNoHistoryFiles() const
{
    return m_fileHistory.GetCount();
}
#endif // WXWIN_COMPATIBILITY_2_6


#if defined(__WXGTK20__)
    #include "wx/gtk/filehistory.h"
#else
    // no platform-specific implementation of wxFileHistory yet
    class WXDLLIMPEXP_CORE wxFileHistory : public wxFileHistoryBase
    {
    public:
        wxFileHistory(size_t maxFiles = 9, wxWindowID idBase = wxID_FILE1)
            : wxFileHistoryBase(maxFiles, idBase) {}

        DECLARE_DYNAMIC_CLASS(wxFileHistory)
    };
#endif

#endif // wxUSE_FILE_HISTORY

#endif // _WX_FILEHISTORY_H_
