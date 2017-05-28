/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dir.cpp
// Purpose:     wxDir implementation for Win32
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.12.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
#endif // PCH

#include "wx/dir.h"

#ifdef __WINDOWS__
    #include "wx/msw/private.h"
#endif

// ----------------------------------------------------------------------------
// define the types and functions used for file searching
// ----------------------------------------------------------------------------

namespace
{

typedef WIN32_FIND_DATA FIND_STRUCT;
typedef HANDLE FIND_DATA;
typedef DWORD FIND_ATTR;

inline FIND_DATA InitFindData()
{
    return INVALID_HANDLE_VALUE;
}

inline bool IsFindDataOk(FIND_DATA fd)
{
        return fd != INVALID_HANDLE_VALUE;
}

inline void FreeFindData(FIND_DATA fd)
{
    if ( !::FindClose(fd) )
    {
        wxLogLastError(wxT("FindClose"));
    }
}

const wxChar *GetNameFromFindData(const FIND_STRUCT *finddata)
{
    return finddata->cFileName;
}

// Helper function checking that the contents of the given FIND_STRUCT really
// match our filter. We need to do it ourselves as native Windows functions
// apply the filter to both the long and the short names of the file, so
// something like "*.bar" matches "foo.bar.baz" too and not only "foo.bar", so
// we have to double check that we have a real match.
inline bool
CheckFoundMatch(const FIND_STRUCT* finddata, const wxString& filter)
{
    // If there is no filter, the found file must be the one we really are
    // looking for.
    if ( filter.empty() )
        return true;

    // Otherwise do check the match validity. Notice that we must do it
    // case-insensitively because the case of the file names is not supposed to
    // matter under Windows.
    wxString fn(GetNameFromFindData(finddata));

    // However if the filter contains only special characters (which is a
    // common case), we can skip the case conversion.
    if ( filter.find_first_not_of(wxS("*?.")) == wxString::npos )
        return fn.Matches(filter);

    return fn.MakeUpper().Matches(filter.Upper());
}

inline bool
FindNext(FIND_DATA fd, const wxString& filter, FIND_STRUCT *finddata)
{
    for ( ;; )
    {
        if ( !::FindNextFile(fd, finddata) )
            return false;

        // If we did find something, check that it really matches.
        if ( CheckFoundMatch(finddata, filter) )
            return true;
    }
}

inline FIND_DATA
FindFirst(const wxString& spec,
          const wxString& filter,
          FIND_STRUCT *finddata)
{
    FIND_DATA fd = ::FindFirstFile(spec.t_str(), finddata);

    // As in FindNext() above, we need to check that the file name we found
    // really matches our filter and look for the next match if it doesn't.
    if ( IsFindDataOk(fd) && !CheckFoundMatch(finddata, filter) )
    {
        if ( !FindNext(fd, filter, finddata) )
        {
            // As we return the invalid handle from here to indicate that we
            // didn't find anything, close the one we initially received
            // ourselves.
            FreeFindData(fd);

            return INVALID_HANDLE_VALUE;
        }
    }

    return fd;
}

inline FIND_ATTR GetAttrFromFindData(FIND_STRUCT *finddata)
{
    return finddata->dwFileAttributes;
}

inline bool IsDir(FIND_ATTR attr)
{
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

inline bool IsHidden(FIND_ATTR attr)
{
    return (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef MAX_PATH
    #define MAX_PATH 260        // from VC++ headers
#endif

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

#define M_DIR       ((wxDirData *)m_data)

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// this class stores everything we need to enumerate the files
class wxDirData
{
public:
    wxDirData(const wxString& dirname);
    ~wxDirData();

    void SetFileSpec(const wxString& filespec) { m_filespec = filespec; }
    void SetFlags(int flags) { m_flags = flags; }

    void Close();
    void Rewind();
    bool Read(wxString *filename);

    const wxString& GetName() const { return m_dirname; }

private:
    FIND_DATA m_finddata;

    wxString m_dirname;
    wxString m_filespec;

    int      m_flags;

    wxDECLARE_NO_COPY_CLASS(wxDirData);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDirData
// ----------------------------------------------------------------------------

wxDirData::wxDirData(const wxString& dirname)
         : m_dirname(dirname)
{
    m_finddata = InitFindData();
}

wxDirData::~wxDirData()
{
    Close();
}

void wxDirData::Close()
{
    if ( IsFindDataOk(m_finddata) )
    {
        FreeFindData(m_finddata);

        m_finddata = InitFindData();
    }
}

void wxDirData::Rewind()
{
    Close();
}

bool wxDirData::Read(wxString *filename)
{
    bool first = false;

    WIN32_FIND_DATA finddata;
    #define PTR_TO_FINDDATA (&finddata)

    if ( !IsFindDataOk(m_finddata) )
    {
        // open first
        wxString filespec = m_dirname;
        if ( !wxEndsWithPathSeparator(filespec) )
        {
            filespec += wxT('\\');
        }
        if ( !m_filespec )
            filespec += wxT("*.*");
        else
            filespec += m_filespec;

        m_finddata = FindFirst(filespec, m_filespec, PTR_TO_FINDDATA);

        first = true;
    }

    if ( !IsFindDataOk(m_finddata) )
    {
#ifdef __WIN32__
        DWORD err = ::GetLastError();

        if ( err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES )
        {
            wxLogSysError(err, _("Cannot enumerate files in directory '%s'"),
                          m_dirname.c_str());
        }
#endif // __WIN32__
        //else: not an error, just no (such) files

        return false;
    }

    const wxChar *name;
    FIND_ATTR attr;

    for ( ;; )
    {
        if ( first )
        {
            first = false;
        }
        else
        {
            if ( !FindNext(m_finddata, m_filespec, PTR_TO_FINDDATA) )
            {
#ifdef __WIN32__
                DWORD err = ::GetLastError();

                if ( err != ERROR_NO_MORE_FILES )
                {
                    wxLogLastError(wxT("FindNext"));
                }
#endif // __WIN32__
                //else: not an error, just no more (such) files

                return false;
            }
        }

        name = GetNameFromFindData(PTR_TO_FINDDATA);
        attr = GetAttrFromFindData(PTR_TO_FINDDATA);

        // don't return "." and ".." unless asked for
        if ( name[0] == wxT('.') &&
             ((name[1] == wxT('.') && name[2] == wxT('\0')) ||
              (name[1] == wxT('\0'))) )
        {
            if ( !(m_flags & wxDIR_DOTDOT) )
                continue;
        }

        // check the type now
        if ( !(m_flags & wxDIR_FILES) && !IsDir(attr) )
        {
            // it's a file, but we don't want them
            continue;
        }
        else if ( !(m_flags & wxDIR_DIRS) && IsDir(attr) )
        {
            // it's a dir, and we don't want it
            continue;
        }

        // finally, check whether it's a hidden file
        if ( !(m_flags & wxDIR_HIDDEN) )
        {
            if ( IsHidden(attr) )
            {
                // it's a hidden file, skip it
                continue;
            }
        }

        *filename = name;

        break;
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxDir construction/destruction
// ----------------------------------------------------------------------------

wxDir::wxDir(const wxString& dirname)
{
    m_data = NULL;

    (void)Open(dirname);
}

bool wxDir::Open(const wxString& dirname)
{
    delete M_DIR;

    // The Unix code does a similar test
    if (wxDirExists(dirname))
    {
        m_data = new wxDirData(dirname);

        return true;
    }
    else
    {
        m_data = NULL;

        return false;
    }
}

bool wxDir::IsOpened() const
{
    return m_data != NULL;
}

wxString wxDir::GetName() const
{
    wxString name;
    if ( m_data )
    {
        name = M_DIR->GetName();
        if ( !name.empty() )
        {
            // bring to canonical Windows form
            name.Replace(wxT("/"), wxT("\\"));

            if ( name.Last() == wxT('\\') )
            {
                // chop off the last (back)slash
                name.Truncate(name.length() - 1);
            }
        }
    }

    return name;
}

void wxDir::Close()
{
    if ( m_data )
    {
        delete m_data;
        m_data = NULL;
    }
}

// ----------------------------------------------------------------------------
// wxDir enumerating
// ----------------------------------------------------------------------------

bool wxDir::GetFirst(wxString *filename,
                     const wxString& filespec,
                     int flags) const
{
    wxCHECK_MSG( IsOpened(), false, wxT("must wxDir::Open() first") );

    M_DIR->Rewind();

    M_DIR->SetFileSpec(filespec);
    M_DIR->SetFlags(flags);

    return GetNext(filename);
}

bool wxDir::GetNext(wxString *filename) const
{
    wxCHECK_MSG( IsOpened(), false, wxT("must wxDir::Open() first") );

    wxCHECK_MSG( filename, false, wxT("bad pointer in wxDir::GetNext()") );

    return M_DIR->Read(filename);
}

// ----------------------------------------------------------------------------
// wxGetDirectoryTimes: used by wxFileName::GetTimes()
// ----------------------------------------------------------------------------

#ifdef __WIN32__

extern bool
wxGetDirectoryTimes(const wxString& dirname,
                    FILETIME *ftAccess, FILETIME *ftCreate, FILETIME *ftMod)
{
#ifdef __WXWINCE__
    // FindFirst() is going to fail
    wxASSERT_MSG( !dirname.empty(),
                  wxT("incorrect directory name format in wxGetDirectoryTimes") );
#else
    // FindFirst() is going to fail
    wxASSERT_MSG( !dirname.empty() && dirname.Last() != wxT('\\'),
                  wxT("incorrect directory name format in wxGetDirectoryTimes") );
#endif

    FIND_STRUCT fs;
    FIND_DATA fd = FindFirst(dirname, wxEmptyString, &fs);
    if ( !IsFindDataOk(fd) )
    {
        return false;
    }

    *ftAccess = fs.ftLastAccessTime;
    *ftCreate = fs.ftCreationTime;
    *ftMod = fs.ftLastWriteTime;

    FindClose(fd);

    return true;
}

#endif // __WIN32__

