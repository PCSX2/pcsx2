/////////////////////////////////////////////////////////////////////////////
// Name:        wx/fs_mem.h
// Purpose:     in-memory file system
// Author:      Vaclav Slavik
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FS_MEM_H_
#define _WX_FS_MEM_H_

#include "wx/defs.h"

#if wxUSE_FILESYSTEM

#include "wx/filesys.h"

#include "wx/hashmap.h"

class wxMemoryFSFile;
WX_DECLARE_STRING_HASH_MAP(wxMemoryFSFile *, wxMemoryFSHash);

#if wxUSE_GUI
    #include "wx/bitmap.h"
#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// wxMemoryFSHandlerBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMemoryFSHandlerBase : public wxFileSystemHandler
{
public:
    wxMemoryFSHandlerBase();
    virtual ~wxMemoryFSHandlerBase();

    // Add file to list of files stored in memory. Stored data (bitmap, text or
    // raw data) will be copied into private memory stream and available under
    // name "memory:" + filename
    static void AddFile(const wxString& filename, const wxString& textdata);
    static void AddFile(const wxString& filename, const void *binarydata, size_t size);
    static void AddFileWithMimeType(const wxString& filename,
                                    const wxString& textdata,
                                    const wxString& mimetype);
    static void AddFileWithMimeType(const wxString& filename,
                                    const void *binarydata, size_t size,
                                    const wxString& mimetype);

    // Remove file from memory FS and free occupied memory
    static void RemoveFile(const wxString& filename);

    virtual bool CanOpen(const wxString& location) wxOVERRIDE;
    virtual wxFSFile* OpenFile(wxFileSystem& fs, const wxString& location) wxOVERRIDE;
    virtual wxString FindFirst(const wxString& spec, int flags = 0) wxOVERRIDE;
    virtual wxString FindNext() wxOVERRIDE;

protected:
    // check that the given file is not already present in m_Hash; logs an
    // error and returns false if it does exist
    static bool CheckDoesntExist(const wxString& filename);

    // the hash map indexed by the names of the files stored in the memory FS
    static wxMemoryFSHash m_Hash;

    // the file name currently being searched for, i.e. the argument of the
    // last FindFirst() call or empty string if FindFirst() hasn't been called
    // yet or FindNext() didn't find anything
    wxString m_findArgument;

    // iterator into m_Hash used by FindFirst/Next(), possibly m_Hash.end() or
    // even invalid (can only be used when m_findArgument is not empty)
    wxMemoryFSHash::const_iterator m_findIter;
};

// ----------------------------------------------------------------------------
// wxMemoryFSHandler
// ----------------------------------------------------------------------------

#if wxUSE_GUI

// add GUI-only operations to the base class
class WXDLLIMPEXP_CORE wxMemoryFSHandler : public wxMemoryFSHandlerBase
{
public:
    // bring the base class versions into the scope, otherwise they would be
    // inaccessible in wxMemoryFSHandler
    // (unfortunately "using" can't be used as gcc 2.95 doesn't have it...)
    static void AddFile(const wxString& filename, const wxString& textdata)
    {
        wxMemoryFSHandlerBase::AddFile(filename, textdata);
    }

    static void AddFile(const wxString& filename,
                        const void *binarydata,
                        size_t size)
    {
        wxMemoryFSHandlerBase::AddFile(filename, binarydata, size);
    }
    static void AddFileWithMimeType(const wxString& filename,
                                    const wxString& textdata,
                                    const wxString& mimetype)
    {
        wxMemoryFSHandlerBase::AddFileWithMimeType(filename,
                                                   textdata,
                                                   mimetype);
    }
    static void AddFileWithMimeType(const wxString& filename,
                                    const void *binarydata, size_t size,
                                    const wxString& mimetype)
    {
        wxMemoryFSHandlerBase::AddFileWithMimeType(filename,
                                                   binarydata, size,
                                                   mimetype);
    }

#if wxUSE_IMAGE
    static void AddFile(const wxString& filename,
                        const wxImage& image,
                        wxBitmapType type);

    static void AddFile(const wxString& filename,
                        const wxBitmap& bitmap,
                        wxBitmapType type);
#endif // wxUSE_IMAGE

};

#else // !wxUSE_GUI

// just the same thing as the base class in wxBase
class WXDLLIMPEXP_BASE wxMemoryFSHandler : public wxMemoryFSHandlerBase
{
};

#endif // wxUSE_GUI/!wxUSE_GUI

#endif // wxUSE_FILESYSTEM

#endif // _WX_FS_MEM_H_

