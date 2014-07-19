/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/dirctrlg.cpp
// Purpose:     wxGenericDirCtrl
// Author:      Harm van der Heijden, Robert Roebling, Julian Smart
// Modified by:
// Created:     12/12/98
// Copyright:   (c) Harm van der Heijden, Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DIRDLG || wxUSE_FILEDLG

#include "wx/generic/dirctrlg.h"

#ifndef WX_PRECOMP
    #include "wx/hash.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/button.h"
    #include "wx/icon.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/choice.h"
    #include "wx/textctrl.h"
    #include "wx/layout.h"
    #include "wx/sizer.h"
    #include "wx/textdlg.h"
    #include "wx/gdicmn.h"
    #include "wx/image.h"
    #include "wx/module.h"
#endif

#include "wx/filename.h"
#include "wx/filefn.h"
#include "wx/imaglist.h"
#include "wx/tokenzr.h"
#include "wx/dir.h"
#include "wx/artprov.h"
#include "wx/mimetype.h"

#if wxUSE_STATLINE
    #include "wx/statline.h"
#endif

#if defined(__WXMAC__)
    #include  "wx/osx/private.h"  // includes mac headers
#endif

#ifdef __WINDOWS__
#include <windows.h>
#include "wx/msw/winundef.h"
#include "wx/volume.h"

// MinGW has _getdrive() and _chdrive(), Cygwin doesn't.
#if defined(__GNUWIN32__) && !defined(__CYGWIN__)
    #define wxHAS_DRIVE_FUNCTIONS
#endif

#ifdef wxHAS_DRIVE_FUNCTIONS
    #include <direct.h>
#endif

#endif // __WINDOWS__

#if defined(__OS2__) || defined(__DOS__)
    #ifdef __OS2__
        #define INCL_BASE
        #include <os2.h>
        #ifndef __EMX__
            #include <direct.h>
        #endif
        #include <stdlib.h>
        #include <ctype.h>
    #endif
#endif // __OS2__

#if defined(__WXMAC__)
//    #include "MoreFilesX.h"
#endif

#ifdef __BORLANDC__
    #include "dos.h"
#endif

extern WXDLLEXPORT_DATA(const char) wxFileSelectorDefaultWildcardStr[];

// If compiled under Windows, this macro can cause problems
#ifdef GetFirstChild
#undef GetFirstChild
#endif

bool wxIsDriveAvailable(const wxString& dirName);

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------

wxDEFINE_EVENT( wxEVT_DIRCTRL_SELECTIONCHANGED, wxTreeEvent );
wxDEFINE_EVENT( wxEVT_DIRCTRL_FILEACTIVATED, wxTreeEvent );

// ----------------------------------------------------------------------------
// wxGetAvailableDrives, for WINDOWS, DOS, OS2, MAC, UNIX (returns "/")
// ----------------------------------------------------------------------------

size_t wxGetAvailableDrives(wxArrayString &paths, wxArrayString &names, wxArrayInt &icon_ids)
{
#ifdef wxHAS_FILESYSTEM_VOLUMES

#ifdef __WXWINCE__
    // No logical drives; return "\"
    paths.Add(wxT("\\"));
    names.Add(wxT("\\"));
    icon_ids.Add(wxFileIconsTable::computer);
#elif defined(__WIN32__) && wxUSE_FSVOLUME
    // TODO: this code (using wxFSVolumeBase) should be used for all platforms
    //       but unfortunately wxFSVolumeBase is not implemented everywhere
    const wxArrayString as = wxFSVolumeBase::GetVolumes();

    for (size_t i = 0; i < as.GetCount(); i++)
    {
        wxString path = as[i];
        wxFSVolume vol(path);
        int imageId;
        switch (vol.GetKind())
        {
            case wxFS_VOL_FLOPPY:
                if ( (path == wxT("a:\\")) || (path == wxT("b:\\")) )
                    imageId = wxFileIconsTable::floppy;
                else
                    imageId = wxFileIconsTable::removeable;
                break;
            case wxFS_VOL_DVDROM:
            case wxFS_VOL_CDROM:
                imageId = wxFileIconsTable::cdrom;
                break;
            case wxFS_VOL_NETWORK:
                if (path[0] == wxT('\\'))
                    continue; // skip "\\computer\folder"
                imageId = wxFileIconsTable::drive;
                break;
            case wxFS_VOL_DISK:
            case wxFS_VOL_OTHER:
            default:
                imageId = wxFileIconsTable::drive;
                break;
        }
        paths.Add(path);
        names.Add(vol.GetDisplayName());
        icon_ids.Add(imageId);
    }
#elif defined(__OS2__)
    APIRET rc;
    ULONG ulDriveNum = 0;
    ULONG ulDriveMap = 0;
    rc = ::DosQueryCurrentDisk(&ulDriveNum, &ulDriveMap);
    if ( rc == 0)
    {
        size_t i = 0;
        while (i < 26)
        {
            if (ulDriveMap & ( 1 << i ))
            {
                const wxString path = wxFileName::GetVolumeString(
                                        'A' + i, wxPATH_GET_SEPARATOR);
                const wxString name = wxFileName::GetVolumeString(
                                        'A' + i, wxPATH_NO_SEPARATOR);

                // Note: If _filesys is unsupported by some compilers,
                //       we can always replace it by DosQueryFSAttach
                char filesysname[20];
#ifdef __WATCOMC__
                ULONG cbBuffer = sizeof(filesysname);
                PFSQBUFFER2 pfsqBuffer = (PFSQBUFFER2)filesysname;
                APIRET rc = ::DosQueryFSAttach(name.fn_str(),0,FSAIL_QUERYNAME,pfsqBuffer,&cbBuffer);
                if (rc != NO_ERROR)
                {
                    filesysname[0] = '\0';
                }
#else
                _filesys(name.fn_str(), filesysname, sizeof(filesysname));
#endif
                /* FAT, LAN, HPFS, CDFS, NFS */
                int imageId;
                if (path == wxT("A:\\") || path == wxT("B:\\"))
                    imageId = wxFileIconsTable::floppy;
                else if (!strcmp(filesysname, "CDFS"))
                    imageId = wxFileIconsTable::cdrom;
                else if (!strcmp(filesysname, "LAN") ||
                         !strcmp(filesysname, "NFS"))
                    imageId = wxFileIconsTable::drive;
                else
                    imageId = wxFileIconsTable::drive;
                paths.Add(path);
                names.Add(name);
                icon_ids.Add(imageId);
            }
            i ++;
        }
    }
#else // !__WIN32__, !__OS2__
    /* If we can switch to the drive, it exists. */
    for ( char drive = 'A'; drive <= 'Z'; drive++ )
    {
        const wxString
            path = wxFileName::GetVolumeString(drive, wxPATH_GET_SEPARATOR);

        if (wxIsDriveAvailable(path))
        {
            paths.Add(path);
            names.Add(wxFileName::GetVolumeString(drive, wxPATH_NO_SEPARATOR));
            icon_ids.Add(drive <= 2 ? wxFileIconsTable::floppy
                                    : wxFileIconsTable::drive);
        }
    }
#endif // __WIN32__/!__WIN32__

#elif defined(__WXMAC__) && wxOSX_USE_COCOA_OR_CARBON

    ItemCount volumeIndex = 1;
    OSErr err = noErr ;

    while( noErr == err )
    {
        HFSUniStr255 volumeName ;
        FSRef fsRef ;
        FSVolumeInfo volumeInfo ;
        err = FSGetVolumeInfo(0, volumeIndex, NULL, kFSVolInfoFlags , &volumeInfo , &volumeName, &fsRef);
        if( noErr == err )
        {
            wxString path = wxMacFSRefToPath( &fsRef ) ;
            wxString name = wxMacHFSUniStrToString( &volumeName ) ;

            if ( (volumeInfo.flags & kFSVolFlagSoftwareLockedMask) || (volumeInfo.flags & kFSVolFlagHardwareLockedMask) )
            {
                icon_ids.Add(wxFileIconsTable::cdrom);
            }
            else
            {
                icon_ids.Add(wxFileIconsTable::drive);
            }
            // todo other removable

            paths.Add(path);
            names.Add(name);
            volumeIndex++ ;
        }
    }

#elif defined(__UNIX__)
    paths.Add(wxT("/"));
    names.Add(wxT("/"));
    icon_ids.Add(wxFileIconsTable::computer);
#else
    #error "Unsupported platform in wxGenericDirCtrl!"
#endif
    wxASSERT_MSG( (paths.GetCount() == names.GetCount()), wxT("The number of paths and their human readable names should be equal in number."));
    wxASSERT_MSG( (paths.GetCount() == icon_ids.GetCount()), wxT("Wrong number of icons for available drives."));
    return paths.GetCount();
}

// ----------------------------------------------------------------------------
// wxIsDriveAvailable
// ----------------------------------------------------------------------------

#if defined(__DOS__)

bool wxIsDriveAvailable(const wxString& dirName)
{
    // FIXME: this method leads to hang up under Watcom for some reason
#ifdef __WATCOMC__
    wxUnusedVar(dirName);
#else
    if ( dirName.length() == 3 && dirName[1u] == wxT(':') )
    {
        wxString dirNameLower(dirName.Lower());
        // VS: always return true for removable media, since Win95 doesn't
        //     like it when MS-DOS app accesses empty floppy drive
        return (dirNameLower[0u] == wxT('a') ||
                dirNameLower[0u] == wxT('b') ||
                wxDirExists(dirNameLower));
    }
    else
#endif
        return true;
}

#elif defined(__WINDOWS__) || defined(__OS2__)

int setdrive(int WXUNUSED_IN_WINCE(drive))
{
#ifdef __WXWINCE__
    return 0;
#elif defined(wxHAS_DRIVE_FUNCTIONS)
    return _chdrive(drive);
#else
    wxChar  newdrive[4];

    if (drive < 1 || drive > 31)
        return -1;
    newdrive[0] = (wxChar)(wxT('A') + drive - 1);
    newdrive[1] = wxT(':');
#ifdef __OS2__
    newdrive[2] = wxT('\\');
    newdrive[3] = wxT('\0');
#else
    newdrive[2] = wxT('\0');
#endif
#if defined(__WINDOWS__)
    if (::SetCurrentDirectory(newdrive))
#else
    // VA doesn't know what LPSTR is and has its own set
    if (!DosSetCurrentDir((PSZ)newdrive))
#endif
        return 0;
    else
        return -1;
#endif // !GNUWIN32
}

bool wxIsDriveAvailable(const wxString& WXUNUSED_IN_WINCE(dirName))
{
#ifdef __WXWINCE__
    return false;
#else
#ifdef __WIN32__
    UINT errorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
    bool success = true;

    // Check if this is a root directory and if so,
    // whether the drive is available.
    if (dirName.length() == 3 && dirName[(size_t)1] == wxT(':'))
    {
        wxString dirNameLower(dirName.Lower());
#ifndef wxHAS_DRIVE_FUNCTIONS
        success = wxDirExists(dirNameLower);
#else
        #if defined(__OS2__)
        // Avoid changing to drive since no media may be inserted.
        if (dirNameLower[(size_t)0] == 'a' || dirNameLower[(size_t)0] == 'b')
            return success;
        #endif
        int currentDrive = _getdrive();
        int thisDrive = (int) (dirNameLower[(size_t)0] - 'a' + 1) ;
        int err = setdrive( thisDrive ) ;
        setdrive( currentDrive );

        if (err == -1)
        {
            success = false;
        }
#endif
    }
#ifdef __WIN32__
    (void) SetErrorMode(errorMode);
#endif

    return success;
#endif
}
#endif // __WINDOWS__ || __OS2__

#endif // wxUSE_DIRDLG || wxUSE_FILEDLG



#if wxUSE_DIRDLG

// Function which is called by quick sort. We want to override the default wxArrayString behaviour,
// and sort regardless of case.
static int wxCMPFUNC_CONV wxDirCtrlStringCompareFunction(const wxString& strFirst, const wxString& strSecond)
{
    return strFirst.CmpNoCase(strSecond);
}

//-----------------------------------------------------------------------------
// wxDirItemData
//-----------------------------------------------------------------------------

wxDirItemData::wxDirItemData(const wxString& path, const wxString& name,
                             bool isDir)
{
    m_path = path;
    m_name = name;
    /* Insert logic to detect hidden files here
     * In UnixLand we just check whether the first char is a dot
     * For FileNameFromPath read LastDirNameInThisPath ;-) */
    // m_isHidden = (bool)(wxFileNameFromPath(*m_path)[0] == '.');
    m_isHidden = false;
    m_isExpanded = false;
    m_isDir = isDir;
}

void wxDirItemData::SetNewDirName(const wxString& path)
{
    m_path = path;
    m_name = wxFileNameFromPath(path);
}

bool wxDirItemData::HasSubDirs() const
{
    if (m_path.empty())
        return false;

    wxDir dir;
    {
        wxLogNull nolog;
        if ( !dir.Open(m_path) )
            return false;
    }

    return dir.HasSubDirs();
}

bool wxDirItemData::HasFiles(const wxString& WXUNUSED(spec)) const
{
    if (m_path.empty())
        return false;

    wxDir dir;
    {
        wxLogNull nolog;
        if ( !dir.Open(m_path) )
            return false;
    }

    return dir.HasFiles();
}

//-----------------------------------------------------------------------------
// wxGenericDirCtrl
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxGenericDirCtrl, wxControl)
  EVT_TREE_ITEM_EXPANDING     (wxID_TREECTRL, wxGenericDirCtrl::OnExpandItem)
  EVT_TREE_ITEM_COLLAPSED     (wxID_TREECTRL, wxGenericDirCtrl::OnCollapseItem)
  EVT_TREE_BEGIN_LABEL_EDIT   (wxID_TREECTRL, wxGenericDirCtrl::OnBeginEditItem)
  EVT_TREE_END_LABEL_EDIT     (wxID_TREECTRL, wxGenericDirCtrl::OnEndEditItem)
  EVT_TREE_SEL_CHANGED        (wxID_TREECTRL, wxGenericDirCtrl::OnTreeSelChange)
  EVT_TREE_ITEM_ACTIVATED     (wxID_TREECTRL, wxGenericDirCtrl::OnItemActivated)
  EVT_SIZE                    (wxGenericDirCtrl::OnSize)
END_EVENT_TABLE()

wxGenericDirCtrl::wxGenericDirCtrl(void)
{
    Init();
}

void wxGenericDirCtrl::ExpandRoot()
{
    ExpandDir(m_rootId); // automatically expand first level

    // Expand and select the default path
    if (!m_defaultPath.empty())
    {
        ExpandPath(m_defaultPath);
    }
#ifdef __UNIX__
    else
    {
        // On Unix, there's only one node under the (hidden) root node. It
        // represents the / path, so the user would always have to expand it;
        // let's do it ourselves
        ExpandPath( wxT("/") );
    }
#endif
}

bool wxGenericDirCtrl::Create(wxWindow *parent,
                              const wxWindowID treeid,
                              const wxString& dir,
                              const wxPoint& pos,
                              const wxSize& size,
                              long style,
                              const wxString& filter,
                              int defaultFilter,
                              const wxString& name)
{
    if (!wxControl::Create(parent, treeid, pos, size, style, wxDefaultValidator, name))
        return false;

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

    Init();

    long treeStyle = wxTR_HAS_BUTTONS;

    // On Windows CE, if you hide the root, you get a crash when
    // attempting to access data for children of the root item.
#ifndef __WXWINCE__
    treeStyle |= wxTR_HIDE_ROOT;
#endif

#ifdef __WXGTK20__
    treeStyle |= wxTR_NO_LINES;
#endif

    if (style & wxDIRCTRL_EDIT_LABELS)
        treeStyle |= wxTR_EDIT_LABELS;

    if (style & wxDIRCTRL_MULTIPLE)
        treeStyle |= wxTR_MULTIPLE;

    if ((style & wxDIRCTRL_3D_INTERNAL) == 0)
        treeStyle |= wxNO_BORDER;

    m_treeCtrl = CreateTreeCtrl(this, wxID_TREECTRL,
                                wxPoint(0,0), GetClientSize(), treeStyle);

    if (!filter.empty() && (style & wxDIRCTRL_SHOW_FILTERS))
        m_filterListCtrl = new wxDirFilterListCtrl(this, wxID_FILTERLISTCTRL);

    m_defaultPath = dir;
    m_filter = filter;

    if (m_filter.empty())
        m_filter = wxFileSelectorDefaultWildcardStr;

    SetFilterIndex(defaultFilter);

    if (m_filterListCtrl)
        m_filterListCtrl->FillFilterList(filter, defaultFilter);

    m_treeCtrl->SetImageList(wxTheFileIconsTable->GetSmallImageList());

    m_showHidden = false;
    wxDirItemData* rootData = new wxDirItemData(wxEmptyString, wxEmptyString, true);

    wxString rootName;

#if defined(__WINDOWS__) || defined(__OS2__) || defined(__DOS__)
    rootName = _("Computer");
#else
    rootName = _("Sections");
#endif

    m_rootId = m_treeCtrl->AddRoot( rootName, 3, -1, rootData);
    m_treeCtrl->SetItemHasChildren(m_rootId);

    ExpandRoot();

    SetInitialSize(size);
    DoResize();

    return true;
}

wxGenericDirCtrl::~wxGenericDirCtrl()
{
}

void wxGenericDirCtrl::Init()
{
    m_showHidden = false;
    m_currentFilter = 0;
    m_currentFilterStr = wxEmptyString; // Default: any file
    m_treeCtrl = NULL;
    m_filterListCtrl = NULL;
}

wxTreeCtrl* wxGenericDirCtrl::CreateTreeCtrl(wxWindow *parent, wxWindowID treeid, const wxPoint& pos, const wxSize& size, long treeStyle)
{
    return new wxTreeCtrl(parent, treeid, pos, size, treeStyle);
}

void wxGenericDirCtrl::ShowHidden( bool show )
{
    if ( m_showHidden == show )
        return;

    m_showHidden = show;

    if ( HasFlag(wxDIRCTRL_MULTIPLE) )
    {
        wxArrayString paths;
        GetPaths(paths);
        ReCreateTree();
        for ( unsigned n = 0; n < paths.size(); n++ )
        {
            ExpandPath(paths[n]);
        }
    }
    else
    {
        wxString path = GetPath();
        ReCreateTree();
        SetPath(path);
    }
}

const wxTreeItemId
wxGenericDirCtrl::AddSection(const wxString& path, const wxString& name, int imageId)
{
    wxDirItemData *dir_item = new wxDirItemData(path,name,true);

    wxTreeItemId treeid = AppendItem( m_rootId, name, imageId, -1, dir_item);

    m_treeCtrl->SetItemHasChildren(treeid);

    return treeid;
}

void wxGenericDirCtrl::SetupSections()
{
    wxArrayString paths, names;
    wxArrayInt icons;

    size_t n, count = wxGetAvailableDrives(paths, names, icons);

#ifdef __WXGTK20__
    wxString home = wxGetHomeDir();
    AddSection( home, _("Home directory"), 1);
    home += wxT("/Desktop");
    AddSection( home, _("Desktop"), 1);
#endif

    for (n = 0; n < count; n++)
        AddSection(paths[n], names[n], icons[n]);
}

void wxGenericDirCtrl::SetFocus()
{
    // we don't need focus ourselves, give it to the tree so that the user
    // could navigate it
    if (m_treeCtrl)
        m_treeCtrl->SetFocus();
}

void wxGenericDirCtrl::OnBeginEditItem(wxTreeEvent &event)
{
    // don't rename the main entry "Sections"
    if (event.GetItem() == m_rootId)
    {
        event.Veto();
        return;
    }

    // don't rename the individual sections
    if (m_treeCtrl->GetItemParent( event.GetItem() ) == m_rootId)
    {
        event.Veto();
        return;
    }
}

void wxGenericDirCtrl::OnEndEditItem(wxTreeEvent &event)
{
    if (event.IsEditCancelled())
        return;

    if ((event.GetLabel().empty()) ||
        (event.GetLabel() == wxT(".")) ||
        (event.GetLabel() == wxT("..")) ||
        (event.GetLabel().Find(wxT('/')) != wxNOT_FOUND) ||
        (event.GetLabel().Find(wxT('\\')) != wxNOT_FOUND) ||
        (event.GetLabel().Find(wxT('|')) != wxNOT_FOUND))
    {
        wxMessageDialog dialog(this, _("Illegal directory name."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        event.Veto();
        return;
    }

    wxTreeItemId treeid = event.GetItem();
    wxDirItemData *data = GetItemData( treeid );
    wxASSERT( data );

    wxString new_name( wxPathOnly( data->m_path ) );
    new_name += wxString(wxFILE_SEP_PATH);
    new_name += event.GetLabel();

    wxLogNull log;

    if (wxFileExists(new_name))
    {
        wxMessageDialog dialog(this, _("File name exists already."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        event.Veto();
    }

    if (wxRenameFile(data->m_path,new_name))
    {
        data->SetNewDirName( new_name );
    }
    else
    {
        wxMessageDialog dialog(this, _("Operation not permitted."), _("Error"), wxOK | wxICON_ERROR );
        dialog.ShowModal();
        event.Veto();
    }
}

void wxGenericDirCtrl::OnTreeSelChange(wxTreeEvent &event)
{
    wxTreeEvent changedEvent(wxEVT_DIRCTRL_SELECTIONCHANGED, GetId());

    changedEvent.SetEventObject(this);
    changedEvent.SetItem(event.GetItem());
    changedEvent.SetClientObject(m_treeCtrl->GetItemData(event.GetItem()));

    if (GetEventHandler()->SafelyProcessEvent(changedEvent) && !changedEvent.IsAllowed())
        event.Veto();
    else
        event.Skip();
}

void wxGenericDirCtrl::OnItemActivated(wxTreeEvent &event)
{
    wxTreeItemId treeid = event.GetItem();
    const wxDirItemData *data = GetItemData(treeid);

    if (data->m_isDir)
    {
        // is dir
        event.Skip();
    }
    else
    {
        // is file
        wxTreeEvent changedEvent(wxEVT_DIRCTRL_FILEACTIVATED, GetId());

        changedEvent.SetEventObject(this);
        changedEvent.SetItem(treeid);
        changedEvent.SetClientObject(m_treeCtrl->GetItemData(treeid));

        if (GetEventHandler()->SafelyProcessEvent(changedEvent) && !changedEvent.IsAllowed())
            event.Veto();
        else
            event.Skip();
    }
}

void wxGenericDirCtrl::OnExpandItem(wxTreeEvent &event)
{
    wxTreeItemId parentId = event.GetItem();

    // VS: this is needed because the event handler is called from wxTreeCtrl
    //     ctor when wxTR_HIDE_ROOT was specified

    if (!m_rootId.IsOk())
        m_rootId = m_treeCtrl->GetRootItem();

    ExpandDir(parentId);
}

void wxGenericDirCtrl::OnCollapseItem(wxTreeEvent &event )
{
    CollapseDir(event.GetItem());
}

void wxGenericDirCtrl::CollapseDir(wxTreeItemId parentId)
{
    wxTreeItemId child;

    wxDirItemData *data = GetItemData(parentId);
    if (!data->m_isExpanded)
        return;

    data->m_isExpanded = false;

    m_treeCtrl->Freeze();
    if (parentId != m_treeCtrl->GetRootItem())
        m_treeCtrl->CollapseAndReset(parentId);
    m_treeCtrl->DeleteChildren(parentId);
    m_treeCtrl->Thaw();
}

void wxGenericDirCtrl::PopulateNode(wxTreeItemId parentId)
{
    wxDirItemData *data = GetItemData(parentId);

    if (data->m_isExpanded)
        return;

    data->m_isExpanded = true;

    if (parentId == m_treeCtrl->GetRootItem())
    {
        SetupSections();
        return;
    }

    wxASSERT(data);

    wxString search,path,filename;

    wxString dirName(data->m_path);

#if (defined(__WINDOWS__) && !defined(__WXWINCE__)) || defined(__DOS__) || defined(__OS2__)
    // Check if this is a root directory and if so,
    // whether the drive is available.
    if (!wxIsDriveAvailable(dirName))
    {
        data->m_isExpanded = false;
        //wxMessageBox(wxT("Sorry, this drive is not available."));
        return;
    }
#endif

    // This may take a longish time. Go to busy cursor
    wxBusyCursor busy;

#if defined(__WINDOWS__) || defined(__DOS__) || defined(__OS2__)
    if (dirName.Last() == ':')
        dirName += wxString(wxFILE_SEP_PATH);
#endif

    wxArrayString dirs;
    wxArrayString filenames;

    wxDir d;
    wxString eachFilename;

    wxLogNull log;
    d.Open(dirName);

    if (d.IsOpened())
    {
        int style = wxDIR_DIRS;
        if (m_showHidden) style |= wxDIR_HIDDEN;
        if (d.GetFirst(& eachFilename, wxEmptyString, style))
        {
            do
            {
                if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                {
                    dirs.Add(eachFilename);
                }
            }
            while (d.GetNext(&eachFilename));
        }
    }
    dirs.Sort(wxDirCtrlStringCompareFunction);

    // Now do the filenames -- but only if we're allowed to
    if (!HasFlag(wxDIRCTRL_DIR_ONLY))
    {
        d.Open(dirName);

        if (d.IsOpened())
        {
            int style = wxDIR_FILES;
            if (m_showHidden) style |= wxDIR_HIDDEN;
            // Process each filter (ex: "JPEG Files (*.jpg;*.jpeg)|*.jpg;*.jpeg")
            wxStringTokenizer strTok;
            wxString curFilter;
            strTok.SetString(m_currentFilterStr,wxT(";"));
            while(strTok.HasMoreTokens())
            {
                curFilter = strTok.GetNextToken();
                if (d.GetFirst(& eachFilename, curFilter, style))
                {
                    do
                    {
                        if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                        {
                            filenames.Add(eachFilename);
                        }
                    }
                    while (d.GetNext(& eachFilename));
                }
            }
        }
        filenames.Sort(wxDirCtrlStringCompareFunction);
    }

    // Now we really know whether we have any children so tell the tree control
    // about it.
    m_treeCtrl->SetItemHasChildren(parentId, !dirs.empty() || !filenames.empty());

    // Add the sorted dirs
    size_t i;
    for (i = 0; i < dirs.GetCount(); i++)
    {
        eachFilename = dirs[i];
        path = dirName;
        if (!wxEndsWithPathSeparator(path))
            path += wxString(wxFILE_SEP_PATH);
        path += eachFilename;

        wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,true);
        wxTreeItemId treeid = AppendItem( parentId, eachFilename,
                                      wxFileIconsTable::folder, -1, dir_item);
        m_treeCtrl->SetItemImage( treeid, wxFileIconsTable::folder_open,
                                  wxTreeItemIcon_Expanded );

        // assume that it does have children by default as it can take a long
        // time to really check for this (think remote drives...)
        //
        // and if we're wrong, we'll correct the icon later if
        // the user really tries to open this item
        m_treeCtrl->SetItemHasChildren(treeid);
    }

    // Add the sorted filenames
    if (!HasFlag(wxDIRCTRL_DIR_ONLY))
    {
        for (i = 0; i < filenames.GetCount(); i++)
        {
            eachFilename = filenames[i];
            path = dirName;
            if (!wxEndsWithPathSeparator(path))
                path += wxString(wxFILE_SEP_PATH);
            path += eachFilename;
            //path = dirName + wxString(wxT("/")) + eachFilename;
            wxDirItemData *dir_item = new wxDirItemData(path,eachFilename,false);
            int image_id = wxFileIconsTable::file;
            if (eachFilename.Find(wxT('.')) != wxNOT_FOUND)
                image_id = wxTheFileIconsTable->GetIconID(eachFilename.AfterLast(wxT('.')));
            (void) AppendItem( parentId, eachFilename, image_id, -1, dir_item);
        }
    }
}

void wxGenericDirCtrl::ExpandDir(wxTreeItemId parentId)
{
    // ExpandDir() will not actually expand the tree node, just populate it
    PopulateNode(parentId);
}

void wxGenericDirCtrl::ReCreateTree()
{
    CollapseDir(m_treeCtrl->GetRootItem());
    ExpandRoot();
}

void wxGenericDirCtrl::CollapseTree()
{
    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_treeCtrl->GetFirstChild(m_rootId, cookie);
    while (child.IsOk())
    {
        CollapseDir(child);
        child = m_treeCtrl->GetNextChild(m_rootId, cookie);
    }
}

// Find the child that matches the first part of 'path'.
// E.g. if a child path is "/usr" and 'path' is "/usr/include"
// then the child for /usr is returned.
wxTreeItemId wxGenericDirCtrl::FindChild(wxTreeItemId parentId, const wxString& path, bool& done)
{
    wxString path2(path);

    // Make sure all separators are as per the current platform
    path2.Replace(wxT("\\"), wxString(wxFILE_SEP_PATH));
    path2.Replace(wxT("/"), wxString(wxFILE_SEP_PATH));

    // Append a separator to foil bogus substring matching
    path2 += wxString(wxFILE_SEP_PATH);

    // In MSW or PM, case is not significant
#if defined(__WINDOWS__) || defined(__DOS__) || defined(__OS2__)
    path2.MakeLower();
#endif

    wxTreeItemIdValue cookie;
    wxTreeItemId childId = m_treeCtrl->GetFirstChild(parentId, cookie);
    while (childId.IsOk())
    {
        wxDirItemData* data = GetItemData(childId);

        if (data && !data->m_path.empty())
        {
            wxString childPath(data->m_path);
            if (!wxEndsWithPathSeparator(childPath))
                childPath += wxString(wxFILE_SEP_PATH);

            // In MSW and PM, case is not significant
#if defined(__WINDOWS__) || defined(__DOS__) || defined(__OS2__)
            childPath.MakeLower();
#endif

            if (childPath.length() <= path2.length())
            {
                wxString path3 = path2.Mid(0, childPath.length());
                if (childPath == path3)
                {
                    if (path3.length() == path2.length())
                        done = true;
                    else
                        done = false;
                    return childId;
                }
            }
        }

        childId = m_treeCtrl->GetNextChild(parentId, cookie);
    }
    wxTreeItemId invalid;
    return invalid;
}

// Try to expand as much of the given path as possible,
// and select the given tree item.
bool wxGenericDirCtrl::ExpandPath(const wxString& path)
{
    bool done = false;
    wxTreeItemId treeid = FindChild(m_rootId, path, done);
    wxTreeItemId lastId = treeid; // The last non-zero treeid
    while (treeid.IsOk() && !done)
    {
        ExpandDir(treeid);

        treeid = FindChild(treeid, path, done);
        if (treeid.IsOk())
            lastId = treeid;
    }
    if (!lastId.IsOk())
        return false;

    wxDirItemData *data = GetItemData(lastId);
    if (data->m_isDir)
    {
        m_treeCtrl->Expand(lastId);
    }
    if (HasFlag(wxDIRCTRL_SELECT_FIRST) && data->m_isDir)
    {
        // Find the first file in this directory
        wxTreeItemIdValue cookie;
        wxTreeItemId childId = m_treeCtrl->GetFirstChild(lastId, cookie);
        bool selectedChild = false;
        while (childId.IsOk())
        {
            data = GetItemData(childId);

            if (data && data->m_path != wxEmptyString && !data->m_isDir)
            {
                m_treeCtrl->SelectItem(childId);
                m_treeCtrl->EnsureVisible(childId);
                selectedChild = true;
                break;
            }
            childId = m_treeCtrl->GetNextChild(lastId, cookie);
        }
        if (!selectedChild)
        {
            m_treeCtrl->SelectItem(lastId);
            m_treeCtrl->EnsureVisible(lastId);
        }
    }
    else
    {
        m_treeCtrl->SelectItem(lastId);
        m_treeCtrl->EnsureVisible(lastId);
    }

    return true;
}


bool wxGenericDirCtrl::CollapsePath(const wxString& path)
{
    bool done           = false;
    wxTreeItemId treeid     = FindChild(m_rootId, path, done);
    wxTreeItemId lastId = treeid; // The last non-zero treeid

    while ( treeid.IsOk() && !done )
    {
        CollapseDir(treeid);

        treeid = FindChild(treeid, path, done);

        if ( treeid.IsOk() )
            lastId = treeid;
    }

    if ( !lastId.IsOk() )
        return false;

    m_treeCtrl->SelectItem(lastId);
    m_treeCtrl->EnsureVisible(lastId);

    return true;
}

wxDirItemData* wxGenericDirCtrl::GetItemData(wxTreeItemId itemId)
{
    return static_cast<wxDirItemData*>(m_treeCtrl->GetItemData(itemId));
}

wxString wxGenericDirCtrl::GetPath(wxTreeItemId itemId) const
{
    const wxDirItemData*
        data = static_cast<wxDirItemData*>(m_treeCtrl->GetItemData(itemId));

    return data->m_path;
}

wxString wxGenericDirCtrl::GetPath() const
{
    // Allow calling GetPath() in multiple selection from OnSelFilter
    if (m_treeCtrl->HasFlag(wxTR_MULTIPLE))
    {
        wxArrayTreeItemIds items;
        m_treeCtrl->GetSelections(items);
        if (items.size() > 0)
        {
            // return first string only
            wxTreeItemId treeid = items[0];
            return GetPath(treeid);
        }

        return wxEmptyString;
    }

    wxTreeItemId treeid = m_treeCtrl->GetSelection();
    if (treeid)
    {
        return GetPath(treeid);
    }
    else
        return wxEmptyString;
}

void wxGenericDirCtrl::GetPaths(wxArrayString& paths) const
{
    paths.clear();

    wxArrayTreeItemIds items;
    m_treeCtrl->GetSelections(items);
    for ( unsigned n = 0; n < items.size(); n++ )
    {
        wxTreeItemId treeid = items[n];
        paths.push_back(GetPath(treeid));
    }
}

wxString wxGenericDirCtrl::GetFilePath() const
{
    wxTreeItemId treeid = m_treeCtrl->GetSelection();
    if (treeid)
    {
        wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(treeid);
        if (data->m_isDir)
            return wxEmptyString;
        else
            return data->m_path;
    }
    else
        return wxEmptyString;
}

void wxGenericDirCtrl::GetFilePaths(wxArrayString& paths) const
{
    paths.clear();

    wxArrayTreeItemIds items;
    m_treeCtrl->GetSelections(items);
    for ( unsigned n = 0; n < items.size(); n++ )
    {
        wxTreeItemId treeid = items[n];
        wxDirItemData* data = (wxDirItemData*) m_treeCtrl->GetItemData(treeid);
        if ( !data->m_isDir )
            paths.Add(data->m_path);
    }
}

void wxGenericDirCtrl::SetPath(const wxString& path)
{
    m_defaultPath = path;
    if (m_rootId)
        ExpandPath(path);
}

void wxGenericDirCtrl::SelectPath(const wxString& path, bool select)
{
    bool done = false;
    wxTreeItemId treeid = FindChild(m_rootId, path, done);
    wxTreeItemId lastId = treeid; // The last non-zero treeid
    while ( treeid.IsOk() && !done )
    {
        treeid = FindChild(treeid, path, done);
        if ( treeid.IsOk() )
            lastId = treeid;
    }
    if ( !lastId.IsOk() )
        return;

    if ( done )
    {
        m_treeCtrl->SelectItem(treeid, select);
    }
}

void wxGenericDirCtrl::SelectPaths(const wxArrayString& paths)
{
    if ( HasFlag(wxDIRCTRL_MULTIPLE) )
    {
        UnselectAll();
        for ( unsigned n = 0; n < paths.size(); n++ )
        {
            SelectPath(paths[n]);
        }
    }
}

void wxGenericDirCtrl::UnselectAll()
{
    m_treeCtrl->UnselectAll();
}

// Not used
#if 0
void wxGenericDirCtrl::FindChildFiles(wxTreeItemId treeid, int dirFlags, wxArrayString& filenames)
{
    wxDirItemData *data = (wxDirItemData *) m_treeCtrl->GetItemData(treeid);

    // This may take a longish time. Go to busy cursor
    wxBusyCursor busy;

    wxASSERT(data);

    wxString search,path,filename;

    wxString dirName(data->m_path);

#if defined(__WINDOWS__) || defined(__OS2__)
    if (dirName.Last() == ':')
        dirName += wxString(wxFILE_SEP_PATH);
#endif

    wxDir d;
    wxString eachFilename;

    wxLogNull log;
    d.Open(dirName);

    if (d.IsOpened())
    {
        if (d.GetFirst(& eachFilename, m_currentFilterStr, dirFlags))
        {
            do
            {
                if ((eachFilename != wxT(".")) && (eachFilename != wxT("..")))
                {
                    filenames.Add(eachFilename);
                }
            }
            while (d.GetNext(& eachFilename)) ;
        }
    }
}
#endif

void wxGenericDirCtrl::SetFilterIndex(int n)
{
    m_currentFilter = n;

    wxString f, d;
    if (ExtractWildcard(m_filter, n, f, d))
        m_currentFilterStr = f;
    else
#ifdef __UNIX__
        m_currentFilterStr = wxT("*");
#else
        m_currentFilterStr = wxT("*.*");
#endif
}

void wxGenericDirCtrl::SetFilter(const wxString& filter)
{
    m_filter = filter;

    if (!filter.empty() && !m_filterListCtrl && HasFlag(wxDIRCTRL_SHOW_FILTERS))
        m_filterListCtrl = new wxDirFilterListCtrl(this, wxID_FILTERLISTCTRL);
    else if (filter.empty() && m_filterListCtrl)
    {
        m_filterListCtrl->Destroy();
        m_filterListCtrl = NULL;
    }

    wxString f, d;
    if (ExtractWildcard(m_filter, m_currentFilter, f, d))
        m_currentFilterStr = f;
    else
#ifdef __UNIX__
        m_currentFilterStr = wxT("*");
#else
        m_currentFilterStr = wxT("*.*");
#endif
    // current filter index is meaningless after filter change, set it to zero
    SetFilterIndex(0);
    if (m_filterListCtrl)
        m_filterListCtrl->FillFilterList(m_filter, 0);
}

// Extract description and actual filter from overall filter string
bool wxGenericDirCtrl::ExtractWildcard(const wxString& filterStr, int n, wxString& filter, wxString& description)
{
    wxArrayString filters, descriptions;
    int count = wxParseCommonDialogsFilter(filterStr, descriptions, filters);
    if (count > 0 && n < count)
    {
        filter = filters[n];
        description = descriptions[n];
        return true;
    }

    return false;
}


void wxGenericDirCtrl::DoResize()
{
    wxSize sz = GetClientSize();
    int verticalSpacing = 3;
    if (m_treeCtrl)
    {
        wxSize filterSz ;
        if (m_filterListCtrl)
        {
            filterSz = m_filterListCtrl->GetSize();
            sz.y -= (filterSz.y + verticalSpacing);
        }
        m_treeCtrl->SetSize(0, 0, sz.x, sz.y);
        if (m_filterListCtrl)
        {
            m_filterListCtrl->SetSize(0, sz.y + verticalSpacing, sz.x, filterSz.y);
            // Don't know why, but this needs refreshing after a resize (wxMSW)
            m_filterListCtrl->Refresh();
        }
    }
}


void wxGenericDirCtrl::OnSize(wxSizeEvent& WXUNUSED(event))
{
    DoResize();
}

wxTreeItemId wxGenericDirCtrl::AppendItem (const wxTreeItemId & parent,
                                           const wxString & text,
                                           int image, int selectedImage,
                                           wxTreeItemData * data)
{
  wxTreeCtrl *treeCtrl = GetTreeCtrl ();

  wxASSERT (treeCtrl);

  if (treeCtrl)
  {
    return treeCtrl->AppendItem (parent, text, image, selectedImage, data);
  }
  else
  {
    return wxTreeItemId();
  }
}


//-----------------------------------------------------------------------------
// wxDirFilterListCtrl
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxDirFilterListCtrl, wxChoice)

BEGIN_EVENT_TABLE(wxDirFilterListCtrl, wxChoice)
    EVT_CHOICE(wxID_ANY, wxDirFilterListCtrl::OnSelFilter)
END_EVENT_TABLE()

bool wxDirFilterListCtrl::Create(wxGenericDirCtrl* parent,
                                 const wxWindowID treeid,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style)
{
    m_dirCtrl = parent;

    // by default our border style is determined by the style of our parent
    if ( !(style & wxBORDER_MASK) )
    {
        style |= parent->HasFlag(wxDIRCTRL_3D_INTERNAL) ? wxBORDER_SUNKEN
                                                        : wxBORDER_NONE;
    }

    return wxChoice::Create(parent, treeid, pos, size, 0, NULL, style);
}

void wxDirFilterListCtrl::Init()
{
    m_dirCtrl = NULL;
}

void wxDirFilterListCtrl::OnSelFilter(wxCommandEvent& WXUNUSED(event))
{
    int sel = GetSelection();

    if (m_dirCtrl->HasFlag(wxDIRCTRL_MULTIPLE))
    {
        wxArrayString paths;
        m_dirCtrl->GetPaths(paths);

        m_dirCtrl->SetFilterIndex(sel);

        // If the filter has changed, the view is out of date, so
        // collapse the tree.
        m_dirCtrl->ReCreateTree();

        // Expand and select the previously selected paths
        for (unsigned int i = 0; i < paths.GetCount(); i++)
        {
            m_dirCtrl->ExpandPath(paths.Item(i));
        }
    }
    else
    {
        wxString currentPath = m_dirCtrl->GetPath();

        m_dirCtrl->SetFilterIndex(sel);
        m_dirCtrl->ReCreateTree();

        // Try to restore the selection, or at least the directory
        m_dirCtrl->ExpandPath(currentPath);
    }
}

void wxDirFilterListCtrl::FillFilterList(const wxString& filter, int defaultFilter)
{
    Clear();
    wxArrayString descriptions, filters;
    size_t n = (size_t) wxParseCommonDialogsFilter(filter, descriptions, filters);

    if (n > 0 && defaultFilter < (int) n)
    {
        for (size_t i = 0; i < n; i++)
            Append(descriptions[i]);
        SetSelection(defaultFilter);
    }
}
#endif // wxUSE_DIRDLG

#if wxUSE_DIRDLG || wxUSE_FILEDLG

// ----------------------------------------------------------------------------
// wxFileIconsTable icons
// ----------------------------------------------------------------------------

#ifndef __WXGTK20__
/* Computer (c) Julian Smart */
static const char* const file_icons_tbl_computer_xpm[] = {
/* columns rows colors chars-per-pixel */
"16 16 42 1",
"r c #4E7FD0",
"$ c #7198D9",
"; c #DCE6F6",
"q c #FFFFFF",
"u c #4A7CCE",
"# c #779DDB",
"w c #95B2E3",
"y c #7FA2DD",
"f c #3263B4",
"= c #EAF0FA",
"< c #B1C7EB",
"% c #6992D7",
"9 c #D9E4F5",
"o c #9BB7E5",
"6 c #F7F9FD",
", c #BED0EE",
"3 c #F0F5FC",
"1 c #A8C0E8",
"  c None",
"0 c #FDFEFF",
"4 c #C4D5F0",
"@ c #81A4DD",
"e c #4377CD",
"- c #E2EAF8",
"i c #9FB9E5",
"> c #CCDAF2",
"+ c #89A9DF",
"s c #5584D1",
"t c #5D89D3",
": c #D2DFF4",
"5 c #FAFCFE",
"2 c #F5F8FD",
"8 c #DFE8F7",
"& c #5E8AD4",
"X c #638ED5",
"a c #CEDCF2",
"p c #90AFE2",
"d c #2F5DA9",
"* c #5282D0",
"7 c #E5EDF9",
". c #A2BCE6",
"O c #8CACE0",
/* pixels */
"                ",
"  .XXXXXXXXXXX  ",
"  oXO++@#$%&*X  ",
"  oX=-;:>,<1%X  ",
"  oX23=-;:4,$X  ",
"  oX5633789:@X  ",
"  oX05623=78+X  ",
"  oXqq05623=OX  ",
"  oX,,,,,<<<$X  ",
"  wXXXXXXXXXXe  ",
"  XrtX%$$y@+O,, ",
"  uyiiiiiiiii@< ",
" ouiiiiiiiiiip<a",
" rustX%$$y@+Ow,,",
" dfffffffffffffd",
"                "
};
#endif // !GTK+ 2

// ----------------------------------------------------------------------------
// wxFileIconsTable & friends
// ----------------------------------------------------------------------------

// global instance of a wxFileIconsTable
wxFileIconsTable* wxTheFileIconsTable = NULL;

// A module to allow icons table cleanup

class wxFileIconsTableModule: public wxModule
{
DECLARE_DYNAMIC_CLASS(wxFileIconsTableModule)
public:
    wxFileIconsTableModule() {}
    bool OnInit() { wxTheFileIconsTable = new wxFileIconsTable; return true; }
    void OnExit()
    {
        wxDELETE(wxTheFileIconsTable);
    }
};

IMPLEMENT_DYNAMIC_CLASS(wxFileIconsTableModule, wxModule)

class wxFileIconEntry : public wxObject
{
public:
    wxFileIconEntry(int i) { iconid = i; }

    int iconid;
};

wxFileIconsTable::wxFileIconsTable()
{
    m_HashTable = NULL;
    m_smallImageList = NULL;
}

wxFileIconsTable::~wxFileIconsTable()
{
    if (m_HashTable)
    {
        WX_CLEAR_HASH_TABLE(*m_HashTable);
        delete m_HashTable;
    }
    if (m_smallImageList) delete m_smallImageList;
}

// delayed initialization - wait until first use (wxArtProv not created yet)
void wxFileIconsTable::Create()
{
    wxCHECK_RET(!m_smallImageList && !m_HashTable, wxT("creating icons twice"));
    m_HashTable = new wxHashTable(wxKEY_STRING);
    m_smallImageList = new wxImageList(16, 16);

    // folder:
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_FOLDER,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // folder_open
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // computer
#ifdef __WXGTK20__
    // GTK24 uses this icon in the file open dialog
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_HARDDISK,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
#else
    m_smallImageList->Add(wxIcon(file_icons_tbl_computer_xpm));
#endif
    // drive
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_HARDDISK,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // cdrom
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_CDROM,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // floppy
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_FLOPPY,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // removeable
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_REMOVABLE,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // file
    m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE,
                                                   wxART_CMN_DIALOG,
                                                   wxSize(16, 16)));
    // executable
    if (GetIconID(wxEmptyString, wxT("application/x-executable")) == file)
    {
        m_smallImageList->Add(wxArtProvider::GetBitmap(wxART_EXECUTABLE_FILE,
                                                       wxART_CMN_DIALOG,
                                                       wxSize(16, 16)));
        delete m_HashTable->Get(wxT("exe"));
        m_HashTable->Delete(wxT("exe"));
        m_HashTable->Put(wxT("exe"), new wxFileIconEntry(executable));
    }
    /* else put into list by GetIconID
       (KDE defines application/x-executable for *.exe and has nice icon)
     */
}

wxImageList *wxFileIconsTable::GetSmallImageList()
{
    if (!m_smallImageList)
        Create();

    return m_smallImageList;
}

#if wxUSE_MIMETYPE && wxUSE_IMAGE && (!defined(__WINDOWS__) || wxUSE_WXDIB)
// VS: we don't need this function w/o wxMimeTypesManager because we'll only have
//     one icon and we won't resize it

static wxBitmap CreateAntialiasedBitmap(const wxImage& img)
{
    const unsigned int size = 16;

    wxImage smallimg (size, size);
    unsigned char *p1, *p2, *ps;
    unsigned char mr = img.GetMaskRed(),
                  mg = img.GetMaskGreen(),
                  mb = img.GetMaskBlue();

    unsigned x, y;
    unsigned sr, sg, sb, smask;

    p1 = img.GetData(), p2 = img.GetData() + 3 * size*2, ps = smallimg.GetData();
    smallimg.SetMaskColour(mr, mr, mr);

    for (y = 0; y < size; y++)
    {
        for (x = 0; x < size; x++)
        {
            sr = sg = sb = smask = 0;
            if (p1[0] != mr || p1[1] != mg || p1[2] != mb)
                sr += p1[0], sg += p1[1], sb += p1[2];
            else smask++;
            p1 += 3;
            if (p1[0] != mr || p1[1] != mg || p1[2] != mb)
                sr += p1[0], sg += p1[1], sb += p1[2];
            else smask++;
            p1 += 3;
            if (p2[0] != mr || p2[1] != mg || p2[2] != mb)
                sr += p2[0], sg += p2[1], sb += p2[2];
            else smask++;
            p2 += 3;
            if (p2[0] != mr || p2[1] != mg || p2[2] != mb)
                sr += p2[0], sg += p2[1], sb += p2[2];
            else smask++;
            p2 += 3;

            if (smask > 2)
                ps[0] = ps[1] = ps[2] = mr;
            else
            {
                ps[0] = (unsigned char)(sr >> 2);
                ps[1] = (unsigned char)(sg >> 2);
                ps[2] = (unsigned char)(sb >> 2);
            }
            ps += 3;
        }
        p1 += size*2 * 3, p2 += size*2 * 3;
    }

    return wxBitmap(smallimg);
}

// This function is currently not unused anymore
#if 0
// finds empty borders and return non-empty area of image:
static wxImage CutEmptyBorders(const wxImage& img)
{
    unsigned char mr = img.GetMaskRed(),
                  mg = img.GetMaskGreen(),
                  mb = img.GetMaskBlue();
    unsigned char *dt = img.GetData(), *dttmp;
    unsigned w = img.GetWidth(), h = img.GetHeight();

    unsigned top, bottom, left, right, i;
    bool empt;

#define MK_DTTMP(x,y)      dttmp = dt + ((x + y * w) * 3)
#define NOEMPTY_PIX(empt)  if (dttmp[0] != mr || dttmp[1] != mg || dttmp[2] != mb) {empt = false; break;}

    for (empt = true, top = 0; empt && top < h; top++)
    {
        MK_DTTMP(0, top);
        for (i = 0; i < w; i++, dttmp+=3)
            NOEMPTY_PIX(empt)
    }
    for (empt = true, bottom = h-1; empt && bottom > top; bottom--)
    {
        MK_DTTMP(0, bottom);
        for (i = 0; i < w; i++, dttmp+=3)
            NOEMPTY_PIX(empt)
    }
    for (empt = true, left = 0; empt && left < w; left++)
    {
        MK_DTTMP(left, 0);
        for (i = 0; i < h; i++, dttmp+=3*w)
            NOEMPTY_PIX(empt)
    }
    for (empt = true, right = w-1; empt && right > left; right--)
    {
        MK_DTTMP(right, 0);
        for (i = 0; i < h; i++, dttmp+=3*w)
            NOEMPTY_PIX(empt)
    }
    top--, left--, bottom++, right++;

    return img.GetSubImage(wxRect(left, top, right - left + 1, bottom - top + 1));
}
#endif // #if 0

#endif // wxUSE_MIMETYPE

int wxFileIconsTable::GetIconID(const wxString& extension, const wxString& mime)
{
    if (!m_smallImageList)
        Create();

#if wxUSE_MIMETYPE
    if (!extension.empty())
    {
        wxFileIconEntry *entry = (wxFileIconEntry*) m_HashTable->Get(extension);
        if (entry) return (entry -> iconid);
    }

    wxFileType *ft = (mime.empty()) ?
                   wxTheMimeTypesManager -> GetFileTypeFromExtension(extension) :
                   wxTheMimeTypesManager -> GetFileTypeFromMimeType(mime);

    wxIconLocation iconLoc;
    wxIcon ic;

    {
        wxLogNull logNull;
        if ( ft && ft->GetIcon(&iconLoc) )
        {
            ic = wxIcon( iconLoc );
        }
    }

    delete ft;

    if ( !ic.IsOk() )
    {
        int newid = file;
        m_HashTable->Put(extension, new wxFileIconEntry(newid));
        return newid;
    }

    wxBitmap bmp;
    bmp.CopyFromIcon(ic);

    if ( !bmp.IsOk() )
    {
        int newid = file;
        m_HashTable->Put(extension, new wxFileIconEntry(newid));
        return newid;
    }

    const unsigned int size = 16;

    int treeid = m_smallImageList->GetImageCount();
    if ((bmp.GetWidth() == (int) size) && (bmp.GetHeight() == (int) size))
    {
        m_smallImageList->Add(bmp);
    }
#if wxUSE_IMAGE && (!defined(__WINDOWS__) || wxUSE_WXDIB)
    else
    {
        wxImage img = bmp.ConvertToImage();

        if ((img.GetWidth() != size*2) || (img.GetHeight() != size*2))
//            m_smallImageList->Add(CreateAntialiasedBitmap(CutEmptyBorders(img).Rescale(size*2, size*2)));
            m_smallImageList->Add(CreateAntialiasedBitmap(img.Rescale(size*2, size*2)));
        else
            m_smallImageList->Add(CreateAntialiasedBitmap(img));
    }
#endif // wxUSE_IMAGE

    m_HashTable->Put(extension, new wxFileIconEntry(treeid));
    return treeid;

#else // !wxUSE_MIMETYPE

    wxUnusedVar(mime);
    if (extension == wxT("exe"))
        return executable;
    else
        return file;
#endif // wxUSE_MIMETYPE/!wxUSE_MIMETYPE
}

#endif // wxUSE_DIRDLG || wxUSE_FILEDLG
