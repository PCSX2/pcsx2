/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dir.h
// Purpose:     wxDir is a class for enumerating the files in a directory
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.12.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIR_H_
#define _WX_DIR_H_

#include "wx/longlong.h"
#include "wx/string.h"
#include "wx/filefn.h"      // for wxS_DIR_DEFAULT

class WXDLLIMPEXP_FWD_BASE wxArrayString;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// These flags affect the behaviour of GetFirst/GetNext() and Traverse().
// They define what types are included in the list of items they produce.
// Note that wxDIR_NO_FOLLOW is relevant only on Unix and ignored under systems
// not supporting symbolic links.
enum wxDirFlags
{
    wxDIR_FILES     = 0x0001,       // include files
    wxDIR_DIRS      = 0x0002,       // include directories
    wxDIR_HIDDEN    = 0x0004,       // include hidden files
    wxDIR_DOTDOT    = 0x0008,       // include '.' and '..'
    wxDIR_NO_FOLLOW = 0x0010,       // don't dereference any symlink

    // by default, enumerate everything except '.' and '..'
    wxDIR_DEFAULT   = wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN
};

// these constants are possible return value of wxDirTraverser::OnDir()
enum wxDirTraverseResult
{
    wxDIR_IGNORE = -1,      // ignore this directory but continue with others
    wxDIR_STOP,             // stop traversing
    wxDIR_CONTINUE          // continue into this directory
};

// ----------------------------------------------------------------------------
// wxDirTraverser: helper class for wxDir::Traverse()
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxDirTraverser
{
public:
    /// a virtual dtor has been provided since this class has virtual members
    virtual ~wxDirTraverser() { }
    // called for each file found by wxDir::Traverse()
    //
    // return wxDIR_STOP or wxDIR_CONTINUE from here (wxDIR_IGNORE doesn't
    // make sense)
    virtual wxDirTraverseResult OnFile(const wxString& filename) = 0;

    // called for each directory found by wxDir::Traverse()
    //
    // return one of the enum elements defined above
    virtual wxDirTraverseResult OnDir(const wxString& dirname) = 0;

    // called for each directory which we couldn't open during our traversal
    // of the directory tree
    //
    // this method can also return either wxDIR_STOP, wxDIR_IGNORE or
    // wxDIR_CONTINUE but the latter is treated specially: it means to retry
    // opening the directory and so may lead to infinite loop if it is
    // returned unconditionally, be careful with this!
    //
    // the base class version always returns wxDIR_IGNORE
    virtual wxDirTraverseResult OnOpenError(const wxString& dirname);
};

// ----------------------------------------------------------------------------
// wxDir: portable equivalent of {open/read/close}dir functions
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxDirData;

class WXDLLIMPEXP_BASE wxDir
{
public:

    // ctors
    // -----

    // default, use Open()
    wxDir() { m_data = NULL; }

    // opens the directory for enumeration, use IsOpened() to test success
    wxDir(const wxString& dir);

    // dtor calls Close() automatically
    ~wxDir() { Close(); }

    // open the directory for enumerating
    bool Open(const wxString& dir);

    // close the directory, Open() can be called again later
    void Close();

    // returns true if the directory was successfully opened
    bool IsOpened() const;

    // get the full name of the directory (without '/' at the end)
    wxString GetName() const;

    // Same as GetName() but does include the trailing separator, unless the
    // string is empty (only for invalid directories).
    wxString GetNameWithSep() const;


    // file enumeration routines
    // -------------------------

    // start enumerating all files matching filespec (or all files if it is
    // empty) and flags, return true on success
    bool GetFirst(wxString *filename,
                  const wxString& filespec = wxEmptyString,
                  int flags = wxDIR_DEFAULT) const;

    // get next file in the enumeration started with GetFirst()
    bool GetNext(wxString *filename) const;

    // return true if this directory has any files in it
    bool HasFiles(const wxString& spec = wxEmptyString) const;

    // return true if this directory has any subdirectories
    bool HasSubDirs(const wxString& spec = wxEmptyString) const;

    // enumerate all files in this directory and its subdirectories
    //
    // return the number of files found
    size_t Traverse(wxDirTraverser& sink,
                    const wxString& filespec = wxEmptyString,
                    int flags = wxDIR_DEFAULT) const;

    // simplest version of Traverse(): get the names of all files under this
    // directory into filenames array, return the number of files
    static size_t GetAllFiles(const wxString& dirname,
                              wxArrayString *files,
                              const wxString& filespec = wxEmptyString,
                              int flags = wxDIR_DEFAULT);

    // check if there any files matching the given filespec under the given
    // directory (i.e. searches recursively), return the file path if found or
    // empty string otherwise
    static wxString FindFirst(const wxString& dirname,
                              const wxString& filespec,
                              int flags = wxDIR_DEFAULT);

#if wxUSE_LONGLONG
    // returns the size of all directories recursively found in given path
    static wxULongLong GetTotalSize(const wxString &dir, wxArrayString *filesSkipped = NULL);
#endif // wxUSE_LONGLONG


    // static utilities for directory management
    // (alias to wxFileName's functions for dirs)
    // -----------------------------------------

    // test for existence of a directory with the given name
    static bool Exists(const wxString& dir);

    static bool Make(const wxString &dir, int perm = wxS_DIR_DEFAULT,
                     int flags = 0);

    static bool Remove(const wxString &dir, int flags = 0);


private:
    friend class wxDirData;

    wxDirData *m_data;

    wxDECLARE_NO_COPY_CLASS(wxDir);
};

#endif // _WX_DIR_H_

