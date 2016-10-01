///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/filectrlg.cpp
// Purpose:     wxGenericFileCtrl Implementation
// Author:      Diaa M. Sami
// Created:     2007-07-07
// Copyright:   (c) Diaa M. Sami
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_FILECTRL

#include "wx/generic/filectrlg.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/sizer.h"
    #include "wx/stattext.h"
    #include "wx/checkbox.h"
    #include "wx/msgdlg.h"
    #include "wx/log.h"
    #include "wx/filedlg.h"
#endif

#include "wx/clntdata.h"
#include "wx/file.h"        // for wxS_IXXX constants only
#include "wx/generic/dirctrlg.h" // for wxFileIconsTable
#include "wx/dir.h"
#include "wx/tokenzr.h"
#include "wx/imaglist.h"

#ifdef __WINDOWS__
    #include "wx/msw/wrapwin.h"
#endif

#if defined(__WXWINCE__)
#define IsTopMostDir(dir) (dir == wxT("\\") || dir == wxT("/"))
#elif (defined(__DOS__) || defined(__WINDOWS__) || defined (__OS2__))
#define IsTopMostDir(dir)   (dir.empty())
#else
#define IsTopMostDir(dir)   (dir == wxT("/"))
#endif


// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

static
int wxCALLBACK wxFileDataNameCompare( wxIntPtr data1, wxIntPtr data2, wxIntPtr sortOrder)
{
     wxFileData *fd1 = (wxFileData *)wxUIntToPtr(data1);
     wxFileData *fd2 = (wxFileData *)wxUIntToPtr(data2);

     if (fd1->GetFileName() == wxT(".."))
         return -sortOrder;
     if (fd2->GetFileName() == wxT(".."))
         return sortOrder;
     if (fd1->IsDir() && !fd2->IsDir())
         return -sortOrder;
     if (fd2->IsDir() && !fd1->IsDir())
         return sortOrder;

     return sortOrder*wxStrcmp( fd1->GetFileName(), fd2->GetFileName() );
}

static
int wxCALLBACK wxFileDataSizeCompare(wxIntPtr data1, wxIntPtr data2, wxIntPtr sortOrder)
{
     wxFileData *fd1 = (wxFileData *)wxUIntToPtr(data1);
     wxFileData *fd2 = (wxFileData *)wxUIntToPtr(data2);

     if (fd1->GetFileName() == wxT(".."))
         return -sortOrder;
     if (fd2->GetFileName() == wxT(".."))
         return sortOrder;
     if (fd1->IsDir() && !fd2->IsDir())
         return -sortOrder;
     if (fd2->IsDir() && !fd1->IsDir())
         return sortOrder;
     if (fd1->IsLink() && !fd2->IsLink())
         return -sortOrder;
     if (fd2->IsLink() && !fd1->IsLink())
         return sortOrder;

     return fd1->GetSize() > fd2->GetSize() ? sortOrder : -sortOrder;
}

static
int wxCALLBACK wxFileDataTypeCompare(wxIntPtr data1, wxIntPtr data2, wxIntPtr sortOrder)
{
     wxFileData *fd1 = (wxFileData *)wxUIntToPtr(data1);
     wxFileData *fd2 = (wxFileData *)wxUIntToPtr(data2);

     if (fd1->GetFileName() == wxT(".."))
         return -sortOrder;
     if (fd2->GetFileName() == wxT(".."))
         return sortOrder;
     if (fd1->IsDir() && !fd2->IsDir())
         return -sortOrder;
     if (fd2->IsDir() && !fd1->IsDir())
         return sortOrder;
     if (fd1->IsLink() && !fd2->IsLink())
         return -sortOrder;
     if (fd2->IsLink() && !fd1->IsLink())
         return sortOrder;

     return sortOrder*wxStrcmp( fd1->GetFileType(), fd2->GetFileType() );
}

static
int wxCALLBACK wxFileDataTimeCompare(wxIntPtr data1, wxIntPtr data2, wxIntPtr sortOrder)
{
     wxFileData *fd1 = (wxFileData *)wxUIntToPtr(data1);
     wxFileData *fd2 = (wxFileData *)wxUIntToPtr(data2);

     if (fd1->GetFileName() == wxT(".."))
         return -sortOrder;
     if (fd2->GetFileName() == wxT(".."))
         return sortOrder;
     if (fd1->IsDir() && !fd2->IsDir())
         return -sortOrder;
     if (fd2->IsDir() && !fd1->IsDir())
         return sortOrder;

     return fd1->GetDateTime().IsLaterThan(fd2->GetDateTime()) ? sortOrder : -sortOrder;
}

// defined in src/generic/dirctrlg.cpp
extern size_t wxGetAvailableDrives(wxArrayString &paths, wxArrayString &names, wxArrayInt &icon_ids);

//-----------------------------------------------------------------------------
//  wxFileData
//-----------------------------------------------------------------------------

wxFileData::wxFileData( const wxString &filePath, const wxString &fileName, fileType type, int image_id )
{
    Init();
    m_fileName = fileName;
    m_filePath = filePath;
    m_type = type;
    m_image = image_id;

    ReadData();
}

void wxFileData::Init()
{
    m_size = 0;
    m_type = wxFileData::is_file;
    m_image = wxFileIconsTable::file;
}

void wxFileData::Copy( const wxFileData& fileData )
{
    m_fileName = fileData.GetFileName();
    m_filePath = fileData.GetFilePath();
    m_size = fileData.GetSize();
    m_dateTime = fileData.GetDateTime();
    m_permissions = fileData.GetPermissions();
    m_type = fileData.GetType();
    m_image = fileData.GetImageId();
}

void wxFileData::ReadData()
{
    if (IsDrive())
    {
        m_size = 0;
        return;
    }

#if defined(__DOS__) || (defined(__WINDOWS__) && !defined(__WXWINCE__)) || defined(__OS2__)
    // c:\.. is a drive don't stat it
    if ((m_fileName == wxT("..")) && (m_filePath.length() <= 5))
    {
        m_type = is_drive;
        m_size = 0;
        return;
    }
#endif // __DOS__ || __WINDOWS__

#ifdef __WXWINCE__

    // WinCE

    DWORD fileAttribs = GetFileAttributes(m_filePath.fn_str());
    m_type |= (fileAttribs & FILE_ATTRIBUTE_DIRECTORY) != 0 ? is_dir : 0;

    wxString p, f, ext;
    wxFileName::SplitPath(m_filePath, & p, & f, & ext);
    if (wxStricmp(ext, wxT("exe")) == 0)
        m_type |= is_exe;

    // Find out size
    m_size = 0;
    HANDLE fileHandle = CreateFile(m_filePath.fn_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        m_size = GetFileSize(fileHandle, 0);
        CloseHandle(fileHandle);
    }

    m_dateTime = wxFileModificationTime(m_filePath);

#else

    // OTHER PLATFORMS

    wxStructStat buff;

#if defined(__UNIX__) && (!defined( __OS2__ ) && !defined(__VMS))
    const bool hasStat = lstat( m_filePath.fn_str(), &buff ) == 0;
    if ( hasStat )
        m_type |= S_ISLNK(buff.st_mode) ? is_link : 0;
#else // no lstat()
    const bool hasStat = wxStat( m_filePath, &buff ) == 0;
#endif

    if ( hasStat )
    {
        m_type |= (buff.st_mode & S_IFDIR) != 0 ? is_dir : 0;
        m_type |= (buff.st_mode & wxS_IXUSR) != 0 ? is_exe : 0;

        m_size = buff.st_size;

        m_dateTime = buff.st_mtime;
    }
#endif
    // __WXWINCE__

#if defined(__UNIX__)
    if ( hasStat )
    {
        m_permissions.Printf(wxT("%c%c%c%c%c%c%c%c%c"),
                             buff.st_mode & wxS_IRUSR ? wxT('r') : wxT('-'),
                             buff.st_mode & wxS_IWUSR ? wxT('w') : wxT('-'),
                             buff.st_mode & wxS_IXUSR ? wxT('x') : wxT('-'),
                             buff.st_mode & wxS_IRGRP ? wxT('r') : wxT('-'),
                             buff.st_mode & wxS_IWGRP ? wxT('w') : wxT('-'),
                             buff.st_mode & wxS_IXGRP ? wxT('x') : wxT('-'),
                             buff.st_mode & wxS_IROTH ? wxT('r') : wxT('-'),
                             buff.st_mode & wxS_IWOTH ? wxT('w') : wxT('-'),
                             buff.st_mode & wxS_IXOTH ? wxT('x') : wxT('-'));
    }
#elif defined(__WIN32__)
    DWORD attribs = ::GetFileAttributes(m_filePath.c_str());
    if (attribs != (DWORD)-1)
    {
        m_permissions.Printf(wxT("%c%c%c%c"),
                             attribs & FILE_ATTRIBUTE_ARCHIVE  ? wxT('A') : wxT(' '),
                             attribs & FILE_ATTRIBUTE_READONLY ? wxT('R') : wxT(' '),
                             attribs & FILE_ATTRIBUTE_HIDDEN   ? wxT('H') : wxT(' '),
                             attribs & FILE_ATTRIBUTE_SYSTEM   ? wxT('S') : wxT(' '));
    }
#endif

    // try to get a better icon
    if (m_image == wxFileIconsTable::file)
    {
        if (m_fileName.Find(wxT('.'), true) != wxNOT_FOUND)
        {
            m_image = wxTheFileIconsTable->GetIconID( m_fileName.AfterLast(wxT('.')));
        } else if (IsExe())
        {
            m_image = wxFileIconsTable::executable;
        }
    }
}

wxString wxFileData::GetFileType() const
{
    if (IsDir())
        return _("<DIR>");
    else if (IsLink())
        return _("<LINK>");
    else if (IsDrive())
        return _("<DRIVE>");
    else if (m_fileName.Find(wxT('.'), true) != wxNOT_FOUND)
        return m_fileName.AfterLast(wxT('.'));

    return wxEmptyString;
}

wxString wxFileData::GetModificationTime() const
{
    // want time as 01:02 so they line up nicely, no %r in WIN32
    return m_dateTime.FormatDate() + wxT(" ") + m_dateTime.Format(wxT("%I:%M:%S %p"));
}

wxString wxFileData::GetHint() const
{
    wxString s = m_filePath;
    s += wxT("  ");

    if (IsDir())
        s += _("<DIR>");
    else if (IsLink())
        s += _("<LINK>");
    else if (IsDrive())
        s += _("<DRIVE>");
    else // plain file
        s += wxString::Format(wxPLURAL("%ld byte", "%ld bytes", m_size),
                              wxLongLong(m_size).ToString().c_str());

    s += wxT(' ');

    if ( !IsDrive() )
    {
        s << GetModificationTime()
          << wxT("  ")
          << m_permissions;
    }

    return s;
}

wxString wxFileData::GetEntry( fileListFieldType num ) const
{
    wxString s;
    switch ( num )
    {
        case FileList_Name:
            s = m_fileName;
            break;

        case FileList_Size:
            if (!IsDir() && !IsLink() && !IsDrive())
                s = wxLongLong(m_size).ToString();
            break;

        case FileList_Type:
            s = GetFileType();
            break;

        case FileList_Time:
            if (!IsDrive())
                s = GetModificationTime();
            break;

#if defined(__UNIX__) || defined(__WIN32__)
        case FileList_Perm:
            s = m_permissions;
            break;
#endif // defined(__UNIX__) || defined(__WIN32__)

        default:
            wxFAIL_MSG( wxT("unexpected field in wxFileData::GetEntry()") );
    }

    return s;
}

void wxFileData::SetNewName( const wxString &filePath, const wxString &fileName )
{
    m_fileName = fileName;
    m_filePath = filePath;
}

void wxFileData::MakeItem( wxListItem &item )
{
    item.m_text = m_fileName;
    item.ClearAttributes();
    if (IsExe())
        item.SetTextColour(*wxRED);
    if (IsDir())
        item.SetTextColour(*wxBLUE);

    item.m_image = m_image;

    if (IsLink())
    {
        wxColour dg = wxTheColourDatabase->Find( wxT("MEDIUM GREY") );
        if ( dg.IsOk() )
            item.SetTextColour(dg);
    }
    item.m_data = wxPtrToUInt(this);
}

//-----------------------------------------------------------------------------
//  wxFileListCtrl
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxFileListCtrl,wxListCtrl)

BEGIN_EVENT_TABLE(wxFileListCtrl,wxListCtrl)
    EVT_LIST_DELETE_ITEM(wxID_ANY, wxFileListCtrl::OnListDeleteItem)
    EVT_LIST_DELETE_ALL_ITEMS(wxID_ANY, wxFileListCtrl::OnListDeleteAllItems)
    EVT_LIST_END_LABEL_EDIT(wxID_ANY, wxFileListCtrl::OnListEndLabelEdit)
    EVT_LIST_COL_CLICK(wxID_ANY, wxFileListCtrl::OnListColClick)
END_EVENT_TABLE()


wxFileListCtrl::wxFileListCtrl()
{
    m_showHidden = false;
    m_sort_forward = true;
    m_sort_field = wxFileData::FileList_Name;
}

wxFileListCtrl::wxFileListCtrl(wxWindow *win,
                       wxWindowID id,
                       const wxString& wild,
                       bool showHidden,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxValidator &validator,
                       const wxString &name)
          : wxListCtrl(win, id, pos, size, style, validator, name),
            m_wild(wild)
{
    wxImageList *imageList = wxTheFileIconsTable->GetSmallImageList();

    SetImageList( imageList, wxIMAGE_LIST_SMALL );

    m_showHidden = showHidden;

    m_sort_forward = true;
    m_sort_field = wxFileData::FileList_Name;

    m_dirName = wxT("*");

    if (style & wxLC_REPORT)
        ChangeToReportMode();
}

void wxFileListCtrl::ChangeToListMode()
{
    ClearAll();
    SetSingleStyle( wxLC_LIST );
    UpdateFiles();
}

void wxFileListCtrl::ChangeToReportMode()
{
    ClearAll();
    SetSingleStyle( wxLC_REPORT );

    // do this since WIN32 does mm/dd/yy UNIX does mm/dd/yyyy
    // don't hardcode since mm/dd is dd/mm elsewhere
    int w, h;
    wxDateTime dt(22, wxDateTime::Dec, 2002, 22, 22, 22);
    wxString txt = dt.FormatDate() + wxT("22") + dt.Format(wxT("%I:%M:%S %p"));
    GetTextExtent(txt, &w, &h);

    InsertColumn( 0, _("Name"), wxLIST_FORMAT_LEFT, w );
    InsertColumn( 1, _("Size"), wxLIST_FORMAT_LEFT, w/2 );
    InsertColumn( 2, _("Type"), wxLIST_FORMAT_LEFT, w/2 );
    InsertColumn( 3, _("Modified"), wxLIST_FORMAT_LEFT, w );
#if defined(__UNIX__)
    GetTextExtent(wxT("Permissions 2"), &w, &h);
    InsertColumn( 4, _("Permissions"), wxLIST_FORMAT_LEFT, w );
#elif defined(__WIN32__)
    GetTextExtent(wxT("Attributes 2"), &w, &h);
    InsertColumn( 4, _("Attributes"), wxLIST_FORMAT_LEFT, w );
#endif

    UpdateFiles();
}

void wxFileListCtrl::ChangeToSmallIconMode()
{
    ClearAll();
    SetSingleStyle( wxLC_SMALL_ICON );
    UpdateFiles();
}

void wxFileListCtrl::ShowHidden( bool show )
{
    m_showHidden = show;
    UpdateFiles();
}

long wxFileListCtrl::Add( wxFileData *fd, wxListItem &item )
{
    long ret = -1;
    item.m_mask = wxLIST_MASK_TEXT + wxLIST_MASK_DATA + wxLIST_MASK_IMAGE;
    fd->MakeItem( item );
    long my_style = GetWindowStyleFlag();
    if (my_style & wxLC_REPORT)
    {
        ret = InsertItem( item );
        for (int i = 1; i < wxFileData::FileList_Max; i++)
            SetItem( item.m_itemId, i, fd->GetEntry((wxFileData::fileListFieldType)i) );
    }
    else if ((my_style & wxLC_LIST) || (my_style & wxLC_SMALL_ICON))
    {
        ret = InsertItem( item );
    }
    return ret;
}

void wxFileListCtrl::UpdateItem(const wxListItem &item)
{
    wxFileData *fd = (wxFileData*)GetItemData(item);
    wxCHECK_RET(fd, wxT("invalid filedata"));

    fd->ReadData();

    SetItemText(item, fd->GetFileName());
    SetItemImage(item, fd->GetImageId());

    if (GetWindowStyleFlag() & wxLC_REPORT)
    {
        for (int i = 1; i < wxFileData::FileList_Max; i++)
            SetItem( item.m_itemId, i, fd->GetEntry((wxFileData::fileListFieldType)i) );
    }
}

void wxFileListCtrl::UpdateFiles()
{
    // don't do anything before ShowModal() call which sets m_dirName
    if ( m_dirName == wxT("*") )
        return;

    wxBusyCursor bcur; // this may take a while...

    DeleteAllItems();

    wxListItem item;
    item.m_itemId = 0;
    item.m_col = 0;

#if (defined(__WINDOWS__) || defined(__DOS__) || defined(__WXMAC__) || defined(__OS2__)) && !defined(__WXWINCE__)
    if ( IsTopMostDir(m_dirName) )
    {
        wxArrayString names, paths;
        wxArrayInt icons;
        const size_t count = wxGetAvailableDrives(paths, names, icons);

        for ( size_t n = 0; n < count; n++ )
        {
            // use paths[n] as the drive name too as our HandleAction() can't
            // deal with the drive names (of the form "System (C:)") currently
            // as it mistakenly treats them as file names
            //
            // it would be preferable to show names, and not paths, in the
            // dialog just as the native dialog does but for this we must:
            //  a) store the item type as item data and modify HandleAction()
            //     to use it instead of wxDirExists() to check whether the item
            //     is a directory
            //  b) store the drives by their drive letters and not their
            //     descriptions as otherwise it's pretty confusing to the user
            wxFileData *fd = new wxFileData(paths[n], paths[n],
                                            wxFileData::is_drive, icons[n]);
            if (Add(fd, item) != -1)
                item.m_itemId++;
            else
                delete fd;
        }
    }
    else
#endif // defined(__DOS__) || defined(__WINDOWS__)
    {
        // Real directory...
        if ( !IsTopMostDir(m_dirName) && !m_dirName.empty() )
        {
            wxString p(wxPathOnly(m_dirName));
#if (defined(__UNIX__) || defined(__WXWINCE__)) && !defined(__OS2__)
            if (p.empty()) p = wxT("/");
#endif // __UNIX__
            wxFileData *fd = new wxFileData(p, wxT(".."), wxFileData::is_dir, wxFileIconsTable::folder);
            if (Add(fd, item) != -1)
                item.m_itemId++;
            else
                delete fd;
        }

        wxString dirname(m_dirName);
#if defined(__DOS__) || defined(__WINDOWS__) || defined(__OS2__)
        if (dirname.length() == 2 && dirname[1u] == wxT(':'))
            dirname << wxT('\\');
#endif // defined(__DOS__) || defined(__WINDOWS__) || defined(__OS2__)

        if (dirname.empty())
            dirname = wxFILE_SEP_PATH;

        wxLogNull logNull;
        wxDir dir(dirname);

        if ( dir.IsOpened() )
        {
            wxString dirPrefix(dirname);
            if (dirPrefix.Last() != wxFILE_SEP_PATH)
                dirPrefix += wxFILE_SEP_PATH;

            int hiddenFlag = m_showHidden ? wxDIR_HIDDEN : 0;

            bool cont;
            wxString f;

            // Get the directories first (not matched against wildcards):
            cont = dir.GetFirst(&f, wxEmptyString, wxDIR_DIRS | hiddenFlag);
            while (cont)
            {
                wxFileData *fd = new wxFileData(dirPrefix + f, f, wxFileData::is_dir, wxFileIconsTable::folder);
                if (Add(fd, item) != -1)
                    item.m_itemId++;
                else
                    delete fd;

                cont = dir.GetNext(&f);
            }

            // Tokenize the wildcard string, so we can handle more than 1
            // search pattern in a wildcard.
            wxStringTokenizer tokenWild(m_wild, wxT(";"));
            while ( tokenWild.HasMoreTokens() )
            {
                cont = dir.GetFirst(&f, tokenWild.GetNextToken(),
                                        wxDIR_FILES | hiddenFlag);
                while (cont)
                {
                    wxFileData *fd = new wxFileData(dirPrefix + f, f, wxFileData::is_file, wxFileIconsTable::file);
                    if (Add(fd, item) != -1)
                        item.m_itemId++;
                    else
                        delete fd;

                    cont = dir.GetNext(&f);
                }
            }
        }
    }

    SortItems(m_sort_field, m_sort_forward);
}

void wxFileListCtrl::SetWild( const wxString &wild )
{
    if (wild.Find(wxT('|')) != wxNOT_FOUND)
        return;

    m_wild = wild;
    UpdateFiles();
}

void wxFileListCtrl::MakeDir()
{
    wxString new_name( _("NewName") );
    wxString path( m_dirName );
    path += wxFILE_SEP_PATH;
    path += new_name;
    if (wxFileExists(path))
    {
        // try NewName0, NewName1 etc.
        int i = 0;
        do {
            new_name = _("NewName");
            wxString num;
            num.Printf( wxT("%d"), i );
            new_name += num;

            path = m_dirName;
            path += wxFILE_SEP_PATH;
            path += new_name;
            i++;
        } while (wxFileExists(path));
    }

    wxLogNull log;
    if (!wxMkdir(path))
    {
        wxMessageDialog dialog(this, _("Operation not permitted."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        return;
    }

    wxFileData *fd = new wxFileData( path, new_name, wxFileData::is_dir, wxFileIconsTable::folder );
    wxListItem item;
    item.m_itemId = 0;
    item.m_col = 0;
    long itemid = Add( fd, item );

    if (itemid != -1)
    {
        SortItems(m_sort_field, m_sort_forward);
        itemid = FindItem( 0, wxPtrToUInt(fd) );
        EnsureVisible( itemid );
        EditLabel( itemid );
    }
    else
        delete fd;
}

void wxFileListCtrl::GoToParentDir()
{
    if (!IsTopMostDir(m_dirName))
    {
        size_t len = m_dirName.length();
        if (wxEndsWithPathSeparator(m_dirName))
            m_dirName.Remove( len-1, 1 );
        wxString fname( wxFileNameFromPath(m_dirName) );
        m_dirName = wxPathOnly( m_dirName );
#if defined(__DOS__) || defined(__WINDOWS__) || defined(__OS2__)
        if (!m_dirName.empty())
        {
            if (m_dirName.Last() == wxT('.'))
                m_dirName = wxEmptyString;
        }
#elif defined(__UNIX__)
        if (m_dirName.empty())
            m_dirName = wxT("/");
#endif
        UpdateFiles();
        long id = FindItem( 0, fname );
        if (id != wxNOT_FOUND)
        {
            SetItemState( id, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
            EnsureVisible( id );
        }
    }
}

void wxFileListCtrl::GoToHomeDir()
{
    wxString s = wxGetUserHome( wxString() );
    GoToDir(s);
}

void wxFileListCtrl::GoToDir( const wxString &dir )
{
    if (!wxDirExists(dir)) return;

    m_dirName = dir;
    UpdateFiles();

    SetItemState( 0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );

    EnsureVisible( 0 );
}

void wxFileListCtrl::FreeItemData(wxListItem& item)
{
    if ( item.m_data )
    {
        wxFileData *fd = (wxFileData*)item.m_data;
        delete fd;

        item.m_data = 0;
    }
}

void wxFileListCtrl::OnListDeleteItem( wxListEvent &event )
{
    FreeItemData(event.m_item);
}

void wxFileListCtrl::OnListDeleteAllItems( wxListEvent & WXUNUSED(event) )
{
    FreeAllItemsData();
}

void wxFileListCtrl::FreeAllItemsData()
{
    wxListItem item;
    item.m_mask = wxLIST_MASK_DATA;

    item.m_itemId = GetNextItem( -1, wxLIST_NEXT_ALL );
    while ( item.m_itemId != -1 )
    {
        GetItem( item );
        FreeItemData(item);
        item.m_itemId = GetNextItem( item.m_itemId, wxLIST_NEXT_ALL );
    }
}

void wxFileListCtrl::OnListEndLabelEdit( wxListEvent &event )
{
    wxFileData *fd = (wxFileData*)event.m_item.m_data;
    wxASSERT( fd );

    if ((event.GetLabel().empty()) ||
        (event.GetLabel() == wxT(".")) ||
        (event.GetLabel() == wxT("..")) ||
        (event.GetLabel().First( wxFILE_SEP_PATH ) != wxNOT_FOUND))
    {
        wxMessageDialog dialog(this, _("Illegal directory name."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        event.Veto();
        return;
    }

    wxString new_name( wxPathOnly( fd->GetFilePath() ) );
    new_name += wxFILE_SEP_PATH;
    new_name += event.GetLabel();

    wxLogNull log;

    if (wxFileExists(new_name))
    {
        wxMessageDialog dialog(this, _("File name exists already."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        event.Veto();
    }

    if (wxRenameFile(fd->GetFilePath(),new_name))
    {
        fd->SetNewName( new_name, event.GetLabel() );

        SetItemState( event.GetItem(), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );

        UpdateItem( event.GetItem() );
        EnsureVisible( event.GetItem() );
    }
    else
    {
        wxMessageDialog dialog(this, _("Operation not permitted."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        event.Veto();
    }
}

void wxFileListCtrl::OnListColClick( wxListEvent &event )
{
    int col = event.GetColumn();

    switch (col)
    {
        case wxFileData::FileList_Name :
        case wxFileData::FileList_Size :
        case wxFileData::FileList_Type :
        case wxFileData::FileList_Time : break;
        default : return;
    }

    if ((wxFileData::fileListFieldType)col == m_sort_field)
        m_sort_forward = !m_sort_forward;
    else
        m_sort_field = (wxFileData::fileListFieldType)col;

    SortItems(m_sort_field, m_sort_forward);
}

void wxFileListCtrl::SortItems(wxFileData::fileListFieldType field, bool forward)
{
    m_sort_field = field;
    m_sort_forward = forward;
    const long sort_dir = forward ? 1 : -1;

    switch (m_sort_field)
    {
        case wxFileData::FileList_Size :
            wxListCtrl::SortItems(wxFileDataSizeCompare, sort_dir);
            break;

        case wxFileData::FileList_Type :
            wxListCtrl::SortItems(wxFileDataTypeCompare, sort_dir);
            break;

        case wxFileData::FileList_Time :
            wxListCtrl::SortItems(wxFileDataTimeCompare, sort_dir);
            break;

        case wxFileData::FileList_Name :
        default :
            wxListCtrl::SortItems(wxFileDataNameCompare, sort_dir);
            break;
    }
}

wxFileListCtrl::~wxFileListCtrl()
{
    // Normally the data are freed via an EVT_LIST_DELETE_ALL_ITEMS event and
    // wxFileListCtrl::OnListDeleteAllItems. But if the event is generated after
    // the destruction of the wxFileListCtrl we need to free any data here:
    FreeAllItemsData();
}

#define  ID_CHOICE        (wxID_FILECTRL + 1)
#define  ID_TEXT          (wxID_FILECTRL + 2)
#define  ID_FILELIST_CTRL (wxID_FILECTRL + 3)
#define  ID_CHECK         (wxID_FILECTRL + 4)

///////////////////////////////////////////////////////////////////////////////
// wxGenericFileCtrl implementation
///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC_CLASS( wxGenericFileCtrl, wxNavigationEnabled<wxControl> )

BEGIN_EVENT_TABLE( wxGenericFileCtrl, wxNavigationEnabled<wxControl> )
    EVT_LIST_ITEM_SELECTED( ID_FILELIST_CTRL, wxGenericFileCtrl::OnSelected )
    EVT_LIST_ITEM_ACTIVATED( ID_FILELIST_CTRL, wxGenericFileCtrl::OnActivated )
    EVT_CHOICE( ID_CHOICE, wxGenericFileCtrl::OnChoiceFilter )
    EVT_TEXT_ENTER( ID_TEXT, wxGenericFileCtrl::OnTextEnter )
    EVT_TEXT( ID_TEXT, wxGenericFileCtrl::OnTextChange )
    EVT_CHECKBOX( ID_CHECK, wxGenericFileCtrl::OnCheck )
END_EVENT_TABLE()

bool wxGenericFileCtrl::Create( wxWindow *parent,
                                wxWindowID id,
                                const wxString& defaultDirectory,
                                const wxString& defaultFileName,
                                const wxString& wildCard,
                                long style,
                                const wxPoint& pos,
                                const wxSize& size,
                                const wxString& name )
{
    this->m_style = style;
    m_inSelected = false;
    m_noSelChgEvent = false;
    m_check = NULL;

    // check that the styles are not contradictory
    wxASSERT_MSG( !( ( m_style & wxFC_SAVE ) && ( m_style & wxFC_OPEN ) ),
                  wxT( "can't specify both wxFC_SAVE and wxFC_OPEN at once" ) );

    wxASSERT_MSG( !( ( m_style & wxFC_SAVE ) && ( m_style & wxFC_MULTIPLE ) ),
                  wxT( "wxFC_MULTIPLE can't be used with wxFC_SAVE" ) );

    wxNavigationEnabled<wxControl>::Create( parent, id,
                                            pos, size,
                                            wxTAB_TRAVERSAL,
                                            wxDefaultValidator,
                                            name );

    m_dir = defaultDirectory;

    m_ignoreChanges = true;

    if ( ( m_dir.empty() ) || ( m_dir == wxT( "." ) ) )
    {
        m_dir = wxGetCwd();
        if ( m_dir.empty() )
            m_dir = wxFILE_SEP_PATH;
    }

    const size_t len = m_dir.length();
    if ( ( len > 1 ) && ( wxEndsWithPathSeparator( m_dir ) ) )
        m_dir.Remove( len - 1, 1 );

    m_filterExtension = wxEmptyString;

    // layout

    const bool is_pda = ( wxSystemSettings::GetScreenType() <= wxSYS_SCREEN_PDA );

    wxBoxSizer *mainsizer = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer *staticsizer = new wxBoxSizer( wxHORIZONTAL );
    if ( is_pda )
        staticsizer->Add( new wxStaticText( this, wxID_ANY, _( "Current directory:" ) ),
                          wxSizerFlags().DoubleBorder(wxRIGHT) );
    m_static = new wxStaticText( this, wxID_ANY, m_dir );
    staticsizer->Add( m_static, 1 );
    mainsizer->Add( staticsizer, wxSizerFlags().Expand().Border());

    long style2 = wxLC_LIST;
    if ( !( m_style & wxFC_MULTIPLE ) )
        style2 |= wxLC_SINGLE_SEL;

#ifdef __WXWINCE__
    style2 |= wxSIMPLE_BORDER;
#else
    style2 |= wxSUNKEN_BORDER;
#endif

    m_list = new wxFileListCtrl( this, ID_FILELIST_CTRL,
                                 wxEmptyString, false,
                                 wxDefaultPosition, wxSize( 400, 140 ),
                                 style2 );

    m_text = new wxTextCtrl( this, ID_TEXT, wxEmptyString,
                             wxDefaultPosition, wxDefaultSize,
                             wxTE_PROCESS_ENTER );
    m_choice = new wxChoice( this, ID_CHOICE );

    if ( is_pda )
    {
        // PDAs have a different screen layout
        mainsizer->Add( m_list, wxSizerFlags( 1 ).Expand().HorzBorder() );

        wxBoxSizer *textsizer = new wxBoxSizer( wxHORIZONTAL );
        textsizer->Add( m_text, wxSizerFlags( 1 ).Centre().Border() );
        textsizer->Add( m_choice, wxSizerFlags( 1 ).Centre().Border() );
        mainsizer->Add( textsizer, wxSizerFlags().Expand() );

    }
    else // !is_pda
    {
        mainsizer->Add( m_list, wxSizerFlags( 1 ).Expand().Border() );
        mainsizer->Add( m_text, wxSizerFlags().Expand().Border() );

        wxBoxSizer *choicesizer = new wxBoxSizer( wxHORIZONTAL );
        choicesizer->Add( m_choice, wxSizerFlags( 1 ).Centre() );

        if ( !( m_style & wxFC_NOSHOWHIDDEN ) )
        {
            m_check = new wxCheckBox( this, ID_CHECK, _( "Show &hidden files" ) );
            choicesizer->Add( m_check, wxSizerFlags().Centre().DoubleBorder(wxLEFT) );
        }

        mainsizer->Add( choicesizer, wxSizerFlags().Expand().Border() );
    }

    SetWildcard( wildCard );

    SetAutoLayout( true );
    SetSizer( mainsizer );

    if ( !is_pda )
    {
        mainsizer->Fit( this );
    }

    m_list->GoToDir( m_dir );
    UpdateControls();
    m_text->SetValue( m_fileName );

    m_ignoreChanges = false;

    // must be after m_ignoreChanges = false
    SetFilename( defaultFileName );

    return true;
}

// NB: there is an unfortunate mismatch between wxFileName and wxFileDialog
//     method names but our GetDirectory() does correspond to wxFileName::
//     GetPath() while our GetPath() is wxFileName::GetFullPath()
wxString wxGenericFileCtrl::GetPath() const
{
    wxASSERT_MSG ( !(m_style & wxFC_MULTIPLE), "use GetPaths() instead" );

    return DoGetFileName().GetFullPath();
}

wxString wxGenericFileCtrl::GetFilename() const
{
    wxASSERT_MSG ( !(m_style & wxFC_MULTIPLE), "use GetFilenames() instead" );

    return DoGetFileName().GetFullName();
}

wxString wxGenericFileCtrl::GetDirectory() const
{
    // don't check for wxFC_MULTIPLE here, this one is probably safe to call in
    // any case as it can be always taken to mean "current directory"
    return DoGetFileName().GetPath();
}

wxFileName wxGenericFileCtrl::DoGetFileName() const
{
    wxFileName fn;

    wxString value = m_text->GetValue();
    if ( value.empty() )
    {
        // nothing in the text control, get the selected file from the list
        wxListItem item;
        item.m_itemId = m_list->GetNextItem(-1, wxLIST_NEXT_ALL,
                                            wxLIST_STATE_SELECTED);

        // ... if anything is selected in the list
        if ( item.m_itemId != wxNOT_FOUND )
        {
            m_list->GetItem(item);

            fn.Assign(m_list->GetDir(), item.m_text);
        }
    }
    else // user entered the value
    {
        // the path can be either absolute or relative
        fn.Assign(value);
        if ( fn.IsRelative() )
            fn.MakeAbsolute(m_list->GetDir());
    }

    return fn;
}

// helper used in DoGetFilenames() and needed because Borland can't compile
// operator?: inline
static inline wxString GetFileNameOrPath(const wxFileName& fn, bool fullPath)
{
    return fullPath ? fn.GetFullPath() : fn.GetFullName();
}

void
wxGenericFileCtrl::DoGetFilenames(wxArrayString& filenames, bool fullPath) const
{
    filenames.clear();

    const wxString dir = m_list->GetDir();

    const wxString value = m_text->GetValue();
    if ( !value.empty() )
    {
        wxFileName fn(value);
        if ( fn.IsRelative() )
            fn.MakeAbsolute(dir);

        filenames.push_back(GetFileNameOrPath(fn, fullPath));
        return;
    }

    const int numSel = m_list->GetSelectedItemCount();
    if ( !numSel )
        return;

    filenames.reserve(numSel);

    wxListItem item;
    item.m_mask = wxLIST_MASK_TEXT;
    item.m_itemId = -1;
    for ( ;; )
    {
        item.m_itemId = m_list->GetNextItem(item.m_itemId, wxLIST_NEXT_ALL,
                                            wxLIST_STATE_SELECTED);

        if ( item.m_itemId == -1 )
            break;

        m_list->GetItem(item);

        const wxFileName fn(dir, item.m_text);
        filenames.push_back(GetFileNameOrPath(fn, fullPath));
    }
}

bool wxGenericFileCtrl::SetDirectory( const wxString& dir )
{
    m_ignoreChanges = true;
    m_list->GoToDir( dir );
    UpdateControls();
    m_ignoreChanges = false;

    return wxFileName( dir ).SameAs( m_list->GetDir() );
}

bool wxGenericFileCtrl::SetFilename( const wxString& name )
{
    const long item = m_list->FindItem( -1, name );

    if ( item == -1 ) // file not found either because it doesn't exist or the
        // current filter doesn't show it.
        return false;

    m_noSelChgEvent = true;

    // Deselect selected items
    {
        const int numSelectedItems = m_list->GetSelectedItemCount();

        if ( numSelectedItems > 0 )
        {
            long itemIndex = -1;

            for ( ;; )
            {
                itemIndex = m_list->GetNextItem( itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
                if ( itemIndex == -1 )
                    break;

                m_list->SetItemState( itemIndex, 0, wxLIST_STATE_SELECTED );
            }
        }
    }

    m_list->SetItemState( item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
    m_list->EnsureVisible( item );

    m_noSelChgEvent = false;

    return true;
}

void wxGenericFileCtrl::DoSetFilterIndex( int filterindex )
{
    wxClientData *pcd = m_choice->GetClientObject( filterindex );
    if ( !pcd )
        return;

    const wxString& str = ((static_cast<wxStringClientData *>(pcd))->GetData());
    m_list->SetWild( str );
    m_filterIndex = filterindex;
    if ( str.Left( 2 ) == wxT( "*." ) )
    {
        m_filterExtension = str.Mid( 1 );
        if ( m_filterExtension == wxT( ".*" ) )
            m_filterExtension.clear();
    }
    else
    {
        m_filterExtension.clear();
    }

    GenerateFilterChangedEvent( this, this );
}

void wxGenericFileCtrl::SetWildcard( const wxString& wildCard )
{
    if ( wildCard.empty() || wildCard == wxFileSelectorDefaultWildcardStr )
    {
        m_wildCard = wxString::Format( _( "All files (%s)|%s" ),
                                       wxFileSelectorDefaultWildcardStr,
                                       wxFileSelectorDefaultWildcardStr );
    }
    else
        m_wildCard = wildCard;

    wxArrayString wildDescriptions, wildFilters;
    const size_t count = wxParseCommonDialogsFilter( m_wildCard,
                         wildDescriptions,
                         wildFilters );
    wxCHECK_RET( count, wxT( "wxFileDialog: bad wildcard string" ) );

    m_choice->Clear();

    for ( size_t n = 0; n < count; n++ )
    {
        m_choice->Append(wildDescriptions[n], new wxStringClientData(wildFilters[n]));
    }

    SetFilterIndex( 0 );
}

void wxGenericFileCtrl::SetFilterIndex( int filterindex )
{
    m_choice->SetSelection( filterindex );

    DoSetFilterIndex( filterindex );
}

void wxGenericFileCtrl::OnChoiceFilter( wxCommandEvent &event )
{
    DoSetFilterIndex( ( int )event.GetInt() );
}

void wxGenericFileCtrl::OnCheck( wxCommandEvent &event )
{
    m_list->ShowHidden( event.GetInt() != 0 );
}

void wxGenericFileCtrl::OnActivated( wxListEvent &event )
{
    HandleAction( event.m_item.m_text );
}

void wxGenericFileCtrl::OnTextEnter( wxCommandEvent &WXUNUSED( event ) )
{
    HandleAction( m_text->GetValue() );
}

void wxGenericFileCtrl::OnTextChange( wxCommandEvent &WXUNUSED( event ) )
{
    if ( !m_ignoreChanges )
    {
        // Clear selections.  Otherwise when the user types in a value they may
        // not get the file whose name they typed.
        if ( m_list->GetSelectedItemCount() > 0 )
        {
            long item = m_list->GetNextItem( -1, wxLIST_NEXT_ALL,
                                             wxLIST_STATE_SELECTED );
            while ( item != -1 )
            {
                m_list->SetItemState( item, 0, wxLIST_STATE_SELECTED );
                item = m_list->GetNextItem( item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
            }
        }
    }
}

void wxGenericFileCtrl::OnSelected( wxListEvent &event )
{
    if ( m_ignoreChanges )
        return;

    if ( m_inSelected )
        return;

    m_inSelected = true;
    const wxString filename( event.m_item.m_text );

#ifdef __WXWINCE__
    // No double-click on most WinCE devices, so do action immediately.
    HandleAction( filename );
#else
    if ( filename == wxT( ".." ) )
    {
        m_inSelected = false;
        return;
    }

    wxString dir = m_list->GetDir();
    if ( !IsTopMostDir( dir ) )
        dir += wxFILE_SEP_PATH;
    dir += filename;
    if ( wxDirExists( dir ) )
    {
        m_inSelected = false;

        return;
    }


    m_ignoreChanges = true;
    m_text->SetValue( filename );

    if ( m_list->GetSelectedItemCount() > 1 )
    {
        m_text->Clear();
    }

    if ( !m_noSelChgEvent )
        GenerateSelectionChangedEvent( this, this );

    m_ignoreChanges = false;
#endif
    m_inSelected = false;
}

void wxGenericFileCtrl::HandleAction( const wxString &fn )
{
    if ( m_ignoreChanges )
        return;

    wxString filename( fn );
    if ( filename.empty() )
    {
        return;
    }
    if ( filename == wxT( "." ) ) return;

    wxString dir = m_list->GetDir();

    // "some/place/" means they want to chdir not try to load "place"
    const bool want_dir = filename.Last() == wxFILE_SEP_PATH;
    if ( want_dir )
        filename = filename.RemoveLast();

    if ( filename == wxT( ".." ) )
    {
        m_ignoreChanges = true;
        m_list->GoToParentDir();

        GenerateFolderChangedEvent( this, this );

        UpdateControls();
        m_ignoreChanges = false;
        return;
    }

#ifdef __UNIX__
    if ( filename == wxT( "~" ) )
    {
        m_ignoreChanges = true;
        m_list->GoToHomeDir();

        GenerateFolderChangedEvent( this, this );

        UpdateControls();
        m_ignoreChanges = false;
        return;
    }

    if ( filename.BeforeFirst( wxT( '/' ) ) == wxT( "~" ) )
    {
        filename = wxString( wxGetUserHome() ) + filename.Remove( 0, 1 );
    }
#endif // __UNIX__

    if ( !( m_style & wxFC_SAVE ) )
    {
        if ( ( filename.Find( wxT( '*' ) ) != wxNOT_FOUND ) ||
                ( filename.Find( wxT( '?' ) ) != wxNOT_FOUND ) )
        {
            if ( filename.Find( wxFILE_SEP_PATH ) != wxNOT_FOUND )
            {
                wxMessageBox( _( "Illegal file specification." ),
                              _( "Error" ), wxOK | wxICON_ERROR, this );
                return;
            }
            m_list->SetWild( filename );
            return;
        }
    }

    if ( !IsTopMostDir( dir ) )
        dir += wxFILE_SEP_PATH;
    if ( !wxIsAbsolutePath( filename ) )
    {
        dir += filename;
        filename = dir;
    }

    if ( wxDirExists( filename ) )
    {
        m_ignoreChanges = true;
        m_list->GoToDir( filename );
        UpdateControls();

        GenerateFolderChangedEvent( this, this );

        m_ignoreChanges = false;
        return;
    }

    // they really wanted a dir, but it doesn't exist
    if ( want_dir )
    {
        wxMessageBox( _( "Directory doesn't exist." ), _( "Error" ),
                      wxOK | wxICON_ERROR, this );
        return;
    }

    // append the default extension to the filename if it doesn't have any
    //
    // VZ: the logic of testing for !wxFileExists() only for the open file
    //     dialog is not entirely clear to me, why don't we allow saving to a
    //     file without extension as well?
    if ( !( m_style & wxFC_OPEN ) || !wxFileExists( filename ) )
    {
        filename = wxFileDialogBase::AppendExtension( filename, m_filterExtension );
        GenerateFileActivatedEvent( this, this, wxFileName( filename ).GetFullName() );
        return;
    }

    GenerateFileActivatedEvent( this, this );
}

bool wxGenericFileCtrl::SetPath( const wxString& path )
{
    if ( !wxFileName::FileExists( ( path ) ) )
        return false;

    wxString ext;
    wxFileName::SplitPath( path, &m_dir, &m_fileName, &ext );
    if ( !ext.empty() )
    {
        m_fileName += wxT( "." );
        m_fileName += ext;
    }

    SetDirectory( m_dir );
    SetFilename( m_fileName );

    return true;
}

void wxGenericFileCtrl::GetPaths( wxArrayString& paths ) const
{
    DoGetFilenames( paths, true );
}

void wxGenericFileCtrl::GetFilenames( wxArrayString& files ) const
{
    DoGetFilenames( files, false );
}

void wxGenericFileCtrl::UpdateControls()
{
    const wxString dir = m_list->GetDir();
    m_static->SetLabel( dir );
}

void wxGenericFileCtrl::GoToParentDir()
{
    m_list->GoToParentDir();
    UpdateControls();
}

void wxGenericFileCtrl::GoToHomeDir()
{
    m_list->GoToHomeDir();
    UpdateControls();
}

#endif // wxUSE_FILECTRL
