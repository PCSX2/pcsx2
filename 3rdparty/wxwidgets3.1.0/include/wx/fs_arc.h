/////////////////////////////////////////////////////////////////////////////
// Name:        wx/fs_arc.h
// Purpose:     Archive file system
// Author:      Vaclav Slavik, Mike Wetherell
// Copyright:   (c) 1999 Vaclav Slavik, (c) 2006 Mike Wetherell
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FS_ARC_H_
#define _WX_FS_ARC_H_

#include "wx/defs.h"

#if wxUSE_FS_ARCHIVE

#include "wx/filesys.h"
#include "wx/hashmap.h"

WX_DECLARE_STRING_HASH_MAP(int, wxArchiveFilenameHashMap);

//---------------------------------------------------------------------------
// wxArchiveFSHandler
//---------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxArchiveFSHandler : public wxFileSystemHandler
{
public:
    wxArchiveFSHandler();
    virtual bool CanOpen(const wxString& location) wxOVERRIDE;
    virtual wxFSFile* OpenFile(wxFileSystem& fs, const wxString& location) wxOVERRIDE;
    virtual wxString FindFirst(const wxString& spec, int flags = 0) wxOVERRIDE;
    virtual wxString FindNext() wxOVERRIDE;
    void Cleanup();
    virtual ~wxArchiveFSHandler();

private:
    class wxArchiveFSCache *m_cache;
    wxFileSystem m_fs;

    // these vars are used by FindFirst/Next:
    class wxArchiveFSCacheData *m_Archive;
    struct wxArchiveFSEntry *m_FindEntry;
    wxString m_Pattern, m_BaseDir, m_ZipFile;
    bool m_AllowDirs, m_AllowFiles;
    wxArchiveFilenameHashMap *m_DirsFound;

    wxString DoFind();

    wxDECLARE_NO_COPY_CLASS(wxArchiveFSHandler);
    wxDECLARE_DYNAMIC_CLASS(wxArchiveFSHandler);
};

#endif // wxUSE_FS_ARCHIVE

#endif // _WX_FS_ARC_H_
