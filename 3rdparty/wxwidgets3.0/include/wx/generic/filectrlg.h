///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/filectrlg.h
// Purpose:     wxGenericFileCtrl Header
// Author:      Diaa M. Sami
// Modified by:
// Created:     Jul-07-2007
// Copyright:   (c) Diaa M. Sami
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_FILECTRL_H_
#define _WX_GENERIC_FILECTRL_H_

#if wxUSE_FILECTRL

#include "wx/containr.h"
#include "wx/listctrl.h"
#include "wx/filectrl.h"
#include "wx/filename.h"

class WXDLLIMPEXP_FWD_CORE wxCheckBox;
class WXDLLIMPEXP_FWD_CORE wxChoice;
class WXDLLIMPEXP_FWD_CORE wxStaticText;
class WXDLLIMPEXP_FWD_CORE wxTextCtrl;

extern WXDLLIMPEXP_DATA_CORE(const char) wxFileSelectorDefaultWildcardStr[];

//-----------------------------------------------------------------------------
//  wxFileData - a class to hold the file info for the wxFileListCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileData
{
public:
    enum fileType
    {
        is_file  = 0x0000,
        is_dir   = 0x0001,
        is_link  = 0x0002,
        is_exe   = 0x0004,
        is_drive = 0x0008
    };

    wxFileData() { Init(); }
    // Full copy constructor
    wxFileData( const wxFileData& fileData ) { Copy(fileData); }
    // Create a filedata from this information
    wxFileData( const wxString &filePath, const wxString &fileName,
                fileType type, int image_id );

    // make a full copy of the other wxFileData
    void Copy( const wxFileData &other );

    // (re)read the extra data about the file from the system
    void ReadData();

    // get the name of the file, dir, drive
    wxString GetFileName() const { return m_fileName; }
    // get the full path + name of the file, dir, path
    wxString GetFilePath() const { return m_filePath; }
    // Set the path + name and name of the item
    void SetNewName( const wxString &filePath, const wxString &fileName );

    // Get the size of the file in bytes
    wxFileOffset GetSize() const { return m_size; }
    // Get the type of file, either file extension or <DIR>, <LINK>, <DRIVE>
    wxString GetFileType() const;
    // get the last modification time
    wxDateTime GetDateTime() const { return m_dateTime; }
    // Get the time as a formatted string
    wxString GetModificationTime() const;
    // in UNIX get rwx for file, in MSW get attributes ARHS
    wxString GetPermissions() const { return m_permissions; }
    // Get the id of the image used in a wxImageList
    int GetImageId() const { return m_image; }

    bool IsFile() const  { return !IsDir() && !IsLink() && !IsDrive(); }
    bool IsDir() const   { return (m_type & is_dir  ) != 0; }
    bool IsLink() const  { return (m_type & is_link ) != 0; }
    bool IsExe() const   { return (m_type & is_exe  ) != 0; }
    bool IsDrive() const { return (m_type & is_drive) != 0; }

    // Get/Set the type of file, file/dir/drive/link
    int GetType() const { return m_type; }

    // the wxFileListCtrl fields in report view
    enum fileListFieldType
    {
        FileList_Name,
        FileList_Size,
        FileList_Type,
        FileList_Time,
#if defined(__UNIX__) || defined(__WIN32__)
        FileList_Perm,
#endif // defined(__UNIX__) || defined(__WIN32__)
        FileList_Max
    };

    // Get the entry for report view of wxFileListCtrl
    wxString GetEntry( fileListFieldType num ) const;

    // Get a string representation of the file info
    wxString GetHint() const;
    // initialize a wxListItem attributes
    void MakeItem( wxListItem &item );

    // operators
    wxFileData& operator = (const wxFileData& fd) { Copy(fd); return *this; }

protected:
    wxString m_fileName;
    wxString   m_filePath;
    wxFileOffset m_size;
    wxDateTime m_dateTime;
    wxString m_permissions;
    int      m_type;
    int      m_image;

private:
    void Init();
};

//-----------------------------------------------------------------------------
//  wxFileListCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileListCtrl : public wxListCtrl
{
public:
    wxFileListCtrl();
    wxFileListCtrl( wxWindow *win,
                wxWindowID id,
                const wxString &wild,
                bool showHidden,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = wxLC_LIST,
                const wxValidator &validator = wxDefaultValidator,
                const wxString &name = wxT("filelist") );
    virtual ~wxFileListCtrl();

    virtual void ChangeToListMode();
    virtual void ChangeToReportMode();
    virtual void ChangeToSmallIconMode();
    virtual void ShowHidden( bool show = true );
    bool GetShowHidden() const { return m_showHidden; }

    virtual long Add( wxFileData *fd, wxListItem &item );
    virtual void UpdateItem(const wxListItem &item);
    virtual void UpdateFiles();
    virtual void MakeDir();
    virtual void GoToParentDir();
    virtual void GoToHomeDir();
    virtual void GoToDir( const wxString &dir );
    virtual void SetWild( const wxString &wild );
    wxString GetWild() const { return m_wild; }
    wxString GetDir() const { return m_dirName; }

    void OnListDeleteItem( wxListEvent &event );
    void OnListDeleteAllItems( wxListEvent &event );
    void OnListEndLabelEdit( wxListEvent &event );
    void OnListColClick( wxListEvent &event );

    virtual void SortItems(wxFileData::fileListFieldType field, bool forward);
    bool GetSortDirection() const { return m_sort_forward; }
    wxFileData::fileListFieldType GetSortField() const { return m_sort_field; }

protected:
    void FreeItemData(wxListItem& item);
    void FreeAllItemsData();

    wxString      m_dirName;
    bool          m_showHidden;
    wxString      m_wild;

    bool m_sort_forward;
    wxFileData::fileListFieldType m_sort_field;

private:
    DECLARE_DYNAMIC_CLASS(wxFileListCtrl)
    DECLARE_EVENT_TABLE()
};

class WXDLLIMPEXP_CORE wxGenericFileCtrl : public wxNavigationEnabled<wxControl>,
                                           public wxFileCtrlBase
{
public:
    wxGenericFileCtrl()
    {
        m_ignoreChanges = false;
    }

    wxGenericFileCtrl ( wxWindow *parent,
                        wxWindowID id,
                        const wxString& defaultDirectory = wxEmptyString,
                        const wxString& defaultFilename = wxEmptyString,
                        const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                        long style = wxFC_DEFAULT_STYLE,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        const wxString& name = wxFileCtrlNameStr )
    {
        m_ignoreChanges = false;
        Create(parent, id, defaultDirectory, defaultFilename, wildCard,
               style, pos, size, name );
    }

    virtual ~wxGenericFileCtrl() {}

    bool Create( wxWindow *parent,
                 wxWindowID id,
                 const wxString& defaultDirectory = wxEmptyString,
                 const wxString& defaultFileName = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFC_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 const wxString& name = wxFileCtrlNameStr );

    virtual void SetWildcard( const wxString& wildCard );
    virtual void SetFilterIndex( int filterindex );
    virtual bool SetDirectory( const wxString& dir );

    // Selects a certain file.
    // In case the filename specified isn't found/couldn't be shown with
    // currently selected filter, false is returned and nothing happens
    virtual bool SetFilename( const wxString& name );

    // Changes to a certain directory and selects a certain file.
    // In case the filename specified isn't found/couldn't be shown with
    // currently selected filter, false is returned and if directory exists
    // it's chdir'ed to
    virtual bool SetPath( const wxString& path );

    virtual wxString GetFilename() const;
    virtual wxString GetDirectory() const;
    virtual wxString GetWildcard() const { return this->m_wildCard; }
    virtual wxString GetPath() const;
    virtual void GetPaths( wxArrayString& paths ) const;
    virtual void GetFilenames( wxArrayString& files ) const;
    virtual int GetFilterIndex() const { return m_filterIndex; }

    virtual bool HasMultipleFileSelection() const
        { return HasFlag(wxFC_MULTIPLE); }
    virtual void ShowHidden(bool show) { m_list->ShowHidden( show ); }

    void GoToParentDir();
    void GoToHomeDir();

    // get the directory currently shown in the control: this can be different
    // from GetDirectory() if the user entered a full path (with a path other
    // than the one currently shown in the control) in the text control
    // manually
    wxString GetShownDirectory() const { return m_list->GetDir(); }

    wxFileListCtrl *GetFileList() { return m_list; }

    void ChangeToReportMode() { m_list->ChangeToReportMode(); }
    void ChangeToListMode() { m_list->ChangeToListMode(); }


private:
    void OnChoiceFilter( wxCommandEvent &event );
    void OnCheck( wxCommandEvent &event );
    void OnActivated( wxListEvent &event );
    void OnTextEnter( wxCommandEvent &WXUNUSED( event ) );
    void OnTextChange( wxCommandEvent &WXUNUSED( event ) );
    void OnSelected( wxListEvent &event );
    void HandleAction( const wxString &fn );

    void DoSetFilterIndex( int filterindex );
    void UpdateControls();

    // the first of these methods can only be used for the controls with single
    // selection (i.e. without wxFC_MULTIPLE style), the second one in any case
    wxFileName DoGetFileName() const;
    void DoGetFilenames( wxArrayString& filenames, bool fullPath ) const;

    int m_style;

    wxString         m_filterExtension;
    wxChoice        *m_choice;
    wxTextCtrl      *m_text;
    wxFileListCtrl  *m_list;
    wxCheckBox      *m_check;
    wxStaticText    *m_static;

    wxString        m_dir;
    wxString        m_fileName;
    wxString        m_wildCard; // wild card in one string as we got it

    int     m_filterIndex;
    bool    m_inSelected;
    bool    m_ignoreChanges;
    bool    m_noSelChgEvent; // suppress selection changed events.

    DECLARE_DYNAMIC_CLASS( wxGenericFileCtrl )
    DECLARE_EVENT_TABLE()
};

#endif // wxUSE_FILECTRL

#endif // _WX_GENERIC_FILECTRL_H_
