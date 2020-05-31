///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dircmn.cpp
// Purpose:     wxDir methods common to all implementations
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.05.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/filefn.h"
    #include "wx/arrstr.h"
#endif //WX_PRECOMP

#include "wx/dir.h"
#include "wx/filename.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDirTraverser
// ----------------------------------------------------------------------------

wxDirTraverseResult
wxDirTraverser::OnOpenError(const wxString& WXUNUSED(dirname))
{
    return wxDIR_IGNORE;
}

// ----------------------------------------------------------------------------
// wxDir::HasFiles() and HasSubDirs()
// ----------------------------------------------------------------------------

// dumb generic implementation

bool wxDir::HasFiles(const wxString& spec) const
{
    wxString s;
    return GetFirst(&s, spec, wxDIR_FILES | wxDIR_HIDDEN);
}

// we have a (much) faster version for Unix
#if (defined(__CYGWIN__) && defined(__WINDOWS__)) || !defined(__UNIX_LIKE__) || defined(__EMX__) || defined(__WINE__)

bool wxDir::HasSubDirs(const wxString& spec) const
{
    wxString s;
    return GetFirst(&s, spec, wxDIR_DIRS | wxDIR_HIDDEN);
}

#endif // !Unix

// ----------------------------------------------------------------------------
// wxDir::GetNameWithSep()
// ----------------------------------------------------------------------------

wxString wxDir::GetNameWithSep() const
{
    // Note that for historical reasons (i.e. because GetName() was there
    // first) we implement this one in terms of GetName() even though it might
    // actually make more sense to reverse this logic.

    wxString name = GetName();
    if ( !name.empty() )
    {
        // Notice that even though GetName() isn't supposed to return the
        // separator, it can still be present for the root directory name.
        if ( name.Last() != wxFILE_SEP_PATH )
            name += wxFILE_SEP_PATH;
    }

    return name;
}

// ----------------------------------------------------------------------------
// wxDir::Traverse()
// ----------------------------------------------------------------------------

size_t wxDir::Traverse(wxDirTraverser& sink,
                       const wxString& filespec,
                       int flags) const
{
    wxCHECK_MSG( IsOpened(), (size_t)-1,
                 wxT("dir must be opened before traversing it") );

    // the total number of files found
    size_t nFiles = 0;

    // the name of this dir with path delimiter at the end
    const wxString prefix = GetNameWithSep();

    // first, recurse into subdirs
    if ( flags & wxDIR_DIRS )
    {
        wxString dirname;
        for ( bool cont = GetFirst(&dirname, wxEmptyString,
                                   (flags & ~(wxDIR_FILES | wxDIR_DOTDOT))
                                   | wxDIR_DIRS);
              cont;
              cont = cont && GetNext(&dirname) )
        {
            const wxString fulldirname = prefix + dirname;

            switch ( sink.OnDir(fulldirname) )
            {
                default:
                    wxFAIL_MSG(wxT("unexpected OnDir() return value") );
                    // fall through

                case wxDIR_STOP:
                    cont = false;
                    break;

                case wxDIR_CONTINUE:
                    {
                        wxDir subdir;

                        // don't give the error messages for the directories
                        // which we can't open: there can be all sorts of good
                        // reason for this (e.g. insufficient privileges) and
                        // this shouldn't be treated as an error -- instead
                        // let the user code decide what to do
                        bool ok;
                        do
                        {
                            wxLogNull noLog;
                            ok = subdir.Open(fulldirname);
                            if ( !ok )
                            {
                                // ask the user code what to do
                                bool tryagain;
                                switch ( sink.OnOpenError(fulldirname) )
                                {
                                    default:
                                        wxFAIL_MSG(wxT("unexpected OnOpenError() return value") );
                                        // fall through

                                    case wxDIR_STOP:
                                        cont = false;
                                        // fall through

                                    case wxDIR_IGNORE:
                                        tryagain = false;
                                        break;

                                    case wxDIR_CONTINUE:
                                        tryagain = true;
                                }

                                if ( !tryagain )
                                    break;
                            }
                        }
                        while ( !ok );

                        if ( ok )
                        {
                            nFiles += subdir.Traverse(sink, filespec, flags);
                        }
                    }
                    break;

                case wxDIR_IGNORE:
                    // nothing to do
                    ;
            }
        }
    }

    // now enum our own files
    if ( flags & wxDIR_FILES )
    {
        flags &= ~wxDIR_DIRS;

        wxString filename;
        bool cont = GetFirst(&filename, filespec, flags);
        while ( cont )
        {
            wxDirTraverseResult res = sink.OnFile(prefix + filename);
            if ( res == wxDIR_STOP )
                break;

            wxASSERT_MSG( res == wxDIR_CONTINUE,
                          wxT("unexpected OnFile() return value") );

            nFiles++;

            cont = GetNext(&filename);
        }
    }

    return nFiles;
}

// ----------------------------------------------------------------------------
// wxDir::GetAllFiles()
// ----------------------------------------------------------------------------

class wxDirTraverserSimple : public wxDirTraverser
{
public:
    wxDirTraverserSimple(wxArrayString& files) : m_files(files) { }

    virtual wxDirTraverseResult OnFile(const wxString& filename)
    {
        m_files.push_back(filename);
        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir(const wxString& WXUNUSED(dirname))
    {
        return wxDIR_CONTINUE;
    }

private:
    wxArrayString& m_files;

    wxDECLARE_NO_COPY_CLASS(wxDirTraverserSimple);
};

/* static */
size_t wxDir::GetAllFiles(const wxString& dirname,
                          wxArrayString *files,
                          const wxString& filespec,
                          int flags)
{
    wxCHECK_MSG( files, (size_t)-1, wxT("NULL pointer in wxDir::GetAllFiles") );

    size_t nFiles = 0;

    wxDir dir(dirname);
    if ( dir.IsOpened() )
    {
        wxDirTraverserSimple traverser(*files);

        nFiles += dir.Traverse(traverser, filespec, flags);
    }

    return nFiles;
}

// ----------------------------------------------------------------------------
// wxDir::FindFirst()
// ----------------------------------------------------------------------------

class wxDirTraverserFindFirst : public wxDirTraverser
{
public:
    wxDirTraverserFindFirst() { }

    virtual wxDirTraverseResult OnFile(const wxString& filename)
    {
        m_file = filename;
        return wxDIR_STOP;
    }

    virtual wxDirTraverseResult OnDir(const wxString& WXUNUSED(dirname))
    {
        return wxDIR_CONTINUE;
    }

    const wxString& GetFile() const
    {
        return m_file;
    }

private:
    wxString m_file;

    wxDECLARE_NO_COPY_CLASS(wxDirTraverserFindFirst);
};

/* static */
wxString wxDir::FindFirst(const wxString& dirname,
                          const wxString& filespec,
                          int flags)
{
    wxDir dir(dirname);
    if ( dir.IsOpened() )
    {
        wxDirTraverserFindFirst traverser;

        dir.Traverse(traverser, filespec, flags | wxDIR_FILES);
        return traverser.GetFile();
    }

    return wxEmptyString;
}


// ----------------------------------------------------------------------------
// wxDir::GetTotalSize()
// ----------------------------------------------------------------------------

#if wxUSE_LONGLONG

class wxDirTraverserSumSize : public wxDirTraverser
{
public:
    wxDirTraverserSumSize() { }

    virtual wxDirTraverseResult OnFile(const wxString& filename)
    {
        // wxFileName::GetSize won't use this class again as
        // we're passing it a file and not a directory;
        // thus we are sure to avoid an endless loop
        wxULongLong sz = wxFileName::GetSize(filename);

        if (sz == wxInvalidSize)
        {
            // if the GetSize() failed (this can happen because e.g. a
            // file is locked by another process), we can proceed but
            // we need to at least warn the user that the resulting
            // final size could be not reliable (if e.g. the locked
            // file is very big).
            m_skippedFiles.Add(filename);
            return wxDIR_CONTINUE;
        }

        m_sz += sz;
        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir(const wxString& WXUNUSED(dirname))
    {
        return wxDIR_CONTINUE;
    }

    wxULongLong GetTotalSize() const
        { return m_sz; }
    const wxArrayString& GetSkippedFiles() const
        { return m_skippedFiles; }

protected:
    wxULongLong m_sz;
    wxArrayString m_skippedFiles;
};

wxULongLong wxDir::GetTotalSize(const wxString &dirname, wxArrayString *filesSkipped)
{
    if (!wxDirExists(dirname))
        return wxInvalidSize;

    // to get the size of this directory and its contents we need
    // to recursively walk it...
    wxDir dir(dirname);
    if ( !dir.IsOpened() )
        return wxInvalidSize;

    wxDirTraverserSumSize traverser;
    if (dir.Traverse(traverser) == (size_t)-1 )
        return wxInvalidSize;

    if (filesSkipped)
        *filesSkipped = traverser.GetSkippedFiles();

    return traverser.GetTotalSize();
}

#endif // wxUSE_LONGLONG

// ----------------------------------------------------------------------------
// wxDir helpers
// ----------------------------------------------------------------------------

/* static */
bool wxDir::Exists(const wxString& dir)
{
    return wxFileName::DirExists(dir);
}

/* static */
bool wxDir::Make(const wxString &dir, int perm, int flags)
{
    return wxFileName::Mkdir(dir, perm, flags);
}

/* static */
bool wxDir::Remove(const wxString &dir, int flags)
{
    return wxFileName::Rmdir(dir, flags);
}

