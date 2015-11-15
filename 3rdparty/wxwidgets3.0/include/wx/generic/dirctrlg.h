/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dirctrlg.h
// Purpose:     wxGenericDirCtrl class
//              Builds on wxDirCtrl class written by Robert Roebling for the
//              wxFile application, modified by Harm van der Heijden.
//              Further modified for Windows.
// Author:      Robert Roebling, Harm van der Heijden, Julian Smart et al
// Modified by:
// Created:     21/3/2000
// Copyright:   (c) Robert Roebling, Harm van der Heijden, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIRCTRL_H_
#define _WX_DIRCTRL_H_

#if wxUSE_DIRDLG

#include "wx/treectrl.h"
#include "wx/dialog.h"
#include "wx/dirdlg.h"
#include "wx/choice.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxTextCtrl;
class WXDLLIMPEXP_FWD_BASE wxHashTable;

//-----------------------------------------------------------------------------
// Extra styles for wxGenericDirCtrl
//-----------------------------------------------------------------------------

enum
{
    // Only allow directory viewing/selection, no files
    wxDIRCTRL_DIR_ONLY       = 0x0010,
    // When setting the default path, select the first file in the directory
    wxDIRCTRL_SELECT_FIRST   = 0x0020,
    // Show the filter list
    wxDIRCTRL_SHOW_FILTERS   = 0x0040,
    // Use 3D borders on internal controls
    wxDIRCTRL_3D_INTERNAL    = 0x0080,
    // Editable labels
    wxDIRCTRL_EDIT_LABELS    = 0x0100,
    // Allow multiple selection
    wxDIRCTRL_MULTIPLE       = 0x0200
};

//-----------------------------------------------------------------------------
// wxDirItemData
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDirItemData : public wxTreeItemData
{
public:
    wxDirItemData(const wxString& path, const wxString& name, bool isDir);
    virtual ~wxDirItemData(){}
    void SetNewDirName(const wxString& path);

    bool HasSubDirs() const;
    bool HasFiles(const wxString& spec = wxEmptyString) const;

    wxString m_path, m_name;
    bool m_isHidden;
    bool m_isExpanded;
    bool m_isDir;
};

//-----------------------------------------------------------------------------
// wxDirCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxDirFilterListCtrl;

class WXDLLIMPEXP_CORE wxGenericDirCtrl: public wxControl
{
public:
    wxGenericDirCtrl();
    wxGenericDirCtrl(wxWindow *parent, const wxWindowID id = wxID_ANY,
              const wxString &dir = wxDirDialogDefaultFolderStr,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = wxDIRCTRL_3D_INTERNAL,
              const wxString& filter = wxEmptyString,
              int defaultFilter = 0,
              const wxString& name = wxTreeCtrlNameStr )
    {
        Init();
        Create(parent, id, dir, pos, size, style, filter, defaultFilter, name);
    }

    bool Create(wxWindow *parent, const wxWindowID id = wxID_ANY,
              const wxString &dir = wxDirDialogDefaultFolderStr,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = wxDIRCTRL_3D_INTERNAL,
              const wxString& filter = wxEmptyString,
              int defaultFilter = 0,
              const wxString& name = wxTreeCtrlNameStr );

    virtual void Init();

    virtual ~wxGenericDirCtrl();

    void OnExpandItem(wxTreeEvent &event );
    void OnCollapseItem(wxTreeEvent &event );
    void OnBeginEditItem(wxTreeEvent &event );
    void OnEndEditItem(wxTreeEvent &event );
    void OnTreeSelChange(wxTreeEvent &event);
    void OnItemActivated(wxTreeEvent &event);
    void OnSize(wxSizeEvent &event );

    // Try to expand as much of the given path as possible.
    virtual bool ExpandPath(const wxString& path);
    // collapse the path
    virtual bool CollapsePath(const wxString& path);

    // Accessors

    virtual inline wxString GetDefaultPath() const { return m_defaultPath; }
    virtual void SetDefaultPath(const wxString& path) { m_defaultPath = path; }

    // Get dir or filename
    virtual wxString GetPath() const;
    virtual void GetPaths(wxArrayString& paths) const;

    // Get selected filename path only (else empty string).
    // I.e. don't count a directory as a selection
    virtual wxString GetFilePath() const;
    virtual void GetFilePaths(wxArrayString& paths) const;
    virtual void SetPath(const wxString& path);

    virtual void SelectPath(const wxString& path, bool select = true);
    virtual void SelectPaths(const wxArrayString& paths);

    virtual void ShowHidden( bool show );
    virtual bool GetShowHidden() { return m_showHidden; }

    virtual wxString GetFilter() const { return m_filter; }
    virtual void SetFilter(const wxString& filter);

    virtual int GetFilterIndex() const { return m_currentFilter; }
    virtual void SetFilterIndex(int n);

    virtual wxTreeItemId GetRootId() { return m_rootId; }

    virtual wxTreeCtrl* GetTreeCtrl() const { return m_treeCtrl; }
    virtual wxDirFilterListCtrl* GetFilterListCtrl() const { return m_filterListCtrl; }

    virtual void UnselectAll();

    // Helper
    virtual void SetupSections();

    // Find the child that matches the first part of 'path'.
    // E.g. if a child path is "/usr" and 'path' is "/usr/include"
    // then the child for /usr is returned.
    // If the path string has been used (we're at the leaf), done is set to true
    virtual wxTreeItemId FindChild(wxTreeItemId parentId, const wxString& path, bool& done);

    wxString GetPath(wxTreeItemId itemId) const;

    // Resize the components of the control
    virtual void DoResize();

    // Collapse & expand the tree, thus re-creating it from scratch:
    virtual void ReCreateTree();

    // Collapse the entire tree
    virtual void CollapseTree();

    // overridden base class methods
    virtual void SetFocus();

protected:
    virtual void ExpandRoot();
    virtual void ExpandDir(wxTreeItemId parentId);
    virtual void CollapseDir(wxTreeItemId parentId);
    virtual const wxTreeItemId AddSection(const wxString& path, const wxString& name, int imageId = 0);
    virtual wxTreeItemId AppendItem (const wxTreeItemId & parent,
                const wxString & text,
                int image = -1, int selectedImage = -1,
                wxTreeItemData * data = NULL);
    //void FindChildFiles(wxTreeItemId id, int dirFlags, wxArrayString& filenames);
    virtual wxTreeCtrl* CreateTreeCtrl(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long treeStyle);

    // Extract description and actual filter from overall filter string
    bool ExtractWildcard(const wxString& filterStr, int n, wxString& filter, wxString& description);

private:
    void PopulateNode(wxTreeItemId node);
    wxDirItemData* GetItemData(wxTreeItemId itemId);

    bool            m_showHidden;
    wxTreeItemId    m_rootId;
    wxString        m_defaultPath; // Starting path
    long            m_styleEx; // Extended style
    wxString        m_filter;  // Wildcards in same format as per wxFileDialog
    int             m_currentFilter; // The current filter index
    wxString        m_currentFilterStr; // Current filter string
    wxTreeCtrl*     m_treeCtrl;
    wxDirFilterListCtrl* m_filterListCtrl;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxGenericDirCtrl)
    wxDECLARE_NO_COPY_CLASS(wxGenericDirCtrl);
};

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_DIRCTRL_SELECTIONCHANGED, wxTreeEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_DIRCTRL_FILEACTIVATED, wxTreeEvent );

#define wx__DECLARE_DIRCTRL_EVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_DIRCTRL_ ## evt, id, wxTreeEventHandler(fn))

#define EVT_DIRCTRL_SELECTIONCHANGED(id, fn) wx__DECLARE_DIRCTRL_EVT(SELECTIONCHANGED, id, fn)
#define EVT_DIRCTRL_FILEACTIVATED(id, fn) wx__DECLARE_DIRCTRL_EVT(FILEACTIVATED, id, fn)

//-----------------------------------------------------------------------------
// wxDirFilterListCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDirFilterListCtrl: public wxChoice
{
public:
    wxDirFilterListCtrl() { Init(); }
    wxDirFilterListCtrl(wxGenericDirCtrl* parent, const wxWindowID id = wxID_ANY,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0)
    {
        Init();
        Create(parent, id, pos, size, style);
    }

    bool Create(wxGenericDirCtrl* parent, const wxWindowID id = wxID_ANY,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0);

    void Init();

    virtual ~wxDirFilterListCtrl() {}

    //// Operations
    void FillFilterList(const wxString& filter, int defaultFilter);

    //// Events
    void OnSelFilter(wxCommandEvent& event);

protected:
    wxGenericDirCtrl*    m_dirCtrl;

    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(wxDirFilterListCtrl)
    wxDECLARE_NO_COPY_CLASS(wxDirFilterListCtrl);
};

#if !defined(__WXMSW__) && !defined(__WXMAC__) && !defined(__WXPM__)
    #define wxDirCtrl wxGenericDirCtrl
#endif

// Symbols for accessing individual controls
#define wxID_TREECTRL          7000
#define wxID_FILTERLISTCTRL    7001

#endif // wxUSE_DIRDLG

//-------------------------------------------------------------------------
// wxFileIconsTable - use wxTheFileIconsTable which is created as necessary
//-------------------------------------------------------------------------

#if wxUSE_DIRDLG || wxUSE_FILEDLG || wxUSE_FILECTRL

class WXDLLIMPEXP_FWD_CORE wxImageList;

class WXDLLIMPEXP_CORE wxFileIconsTable
{
public:
    wxFileIconsTable();
    ~wxFileIconsTable();

    enum iconId_Type
    {
        folder,
        folder_open,
        computer,
        drive,
        cdrom,
        floppy,
        removeable,
        file,
        executable
    };

    int GetIconID(const wxString& extension, const wxString& mime = wxEmptyString);
    wxImageList *GetSmallImageList();

protected:
    void Create();  // create on first use

    wxImageList *m_smallImageList;
    wxHashTable *m_HashTable;
};

// The global fileicons table
extern WXDLLIMPEXP_DATA_CORE(wxFileIconsTable *) wxTheFileIconsTable;

#endif // wxUSE_DIRDLG || wxUSE_FILEDLG || wxUSE_FILECTRL

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_DIRCTRL_SELECTIONCHANGED wxEVT_DIRCTRL_SELECTIONCHANGED
#define wxEVT_COMMAND_DIRCTRL_FILEACTIVATED   wxEVT_DIRCTRL_FILEACTIVATED

#endif
    // _WX_DIRCTRLG_H_
