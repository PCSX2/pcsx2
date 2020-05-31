/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fswatchercmn.cpp
// Purpose:     wxMswFileSystemWatcher
// Author:      Bartosz Bekier
// Created:     2009-05-26
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FSWATCHER

#include "wx/fswatcher.h"
#include "wx/private/fswatcher.h"

// ============================================================================
// helpers
// ============================================================================

wxDEFINE_EVENT(wxEVT_FSWATCHER, wxFileSystemWatcherEvent);

static wxString GetFSWEventChangeTypeName(int type)
{
    switch (type)
    {
    case wxFSW_EVENT_CREATE:
        return "CREATE";
    case wxFSW_EVENT_DELETE:
        return "DELETE";
    case wxFSW_EVENT_RENAME:
        return "RENAME";
    case wxFSW_EVENT_MODIFY:
        return "MODIFY";
    case wxFSW_EVENT_ACCESS:
        return "ACCESS";
    case wxFSW_EVENT_ATTRIB: // Currently this is wxGTK-only
        return "ATTRIBUTE";
#ifdef wxHAS_INOTIFY
    case wxFSW_EVENT_UNMOUNT: // Currently this is wxGTK-only
        return "UNMOUNT";
#endif
    case wxFSW_EVENT_WARNING:
        return "WARNING";
    case wxFSW_EVENT_ERROR:
        return "ERROR";
    }

    // should never be reached!
    wxFAIL_MSG("Unknown change type");
    return "INVALID_TYPE";
}


// ============================================================================
// wxFileSystemWatcherEvent implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxFileSystemWatcherEvent, wxEvent);

wxString wxFileSystemWatcherEvent::ToString() const
{
    if (IsError())
    {
        return wxString::Format("FSW_EVT type=%d (%s) message='%s'", m_changeType,
            GetFSWEventChangeTypeName(m_changeType), GetErrorDescription());
    }
    return wxString::Format("FSW_EVT type=%d (%s) path='%s'", m_changeType,
            GetFSWEventChangeTypeName(m_changeType), GetPath().GetFullPath());
}


// ============================================================================
// wxFileSystemWatcherEvent implementation
// ============================================================================

wxFileSystemWatcherBase::wxFileSystemWatcherBase() :
    m_service(0), m_owner(this)
{
}

wxFileSystemWatcherBase::~wxFileSystemWatcherBase()
{
    RemoveAll();
    if (m_service)
    {
        delete m_service;
    }
}

bool wxFileSystemWatcherBase::Add(const wxFileName& path, int events)
{
    wxFSWPathType type = wxFSWPath_None;
    if ( path.FileExists() )
    {
        type = wxFSWPath_File;
    }
    else if ( path.DirExists() )
    {
        type = wxFSWPath_Dir;
    }
    else
    {
        // Don't overreact to being passed a non-existent item. It may have
        // only just been deleted, in which case doing nothing is correct
        wxLogTrace(wxTRACE_FSWATCHER,
                   "Can't monitor non-existent path \"%s\" for changes.",
                   path.GetFullPath());
        return false;
    }

    return AddAny(path, events, type);
}

bool
wxFileSystemWatcherBase::AddAny(const wxFileName& path,
                                int events,
                                wxFSWPathType type,
                                const wxString& filespec)
{
    wxString canonical = GetCanonicalPath(path);
    if (canonical.IsEmpty())
        return false;

    // adding a path in a platform specific way
    wxFSWatchInfo watch(canonical, events, type, filespec);
    if ( !m_service->Add(watch) )
        return false;

    // on success, either add path to our 'watch-list'
    // or, if already watched, inc the refcount. This may happen if
    // a dir is Add()ed, then later AddTree() is called on a parent dir
    wxFSWatchInfoMap::iterator it = m_watches.find(canonical);
    if ( it == m_watches.end() )
    {
        wxFSWatchInfoMap::value_type val(canonical, watch);
        m_watches.insert(val);
    }
    else
    {
        wxFSWatchInfo& watch2 = it->second;
        const int count = watch2.IncRef();
        wxLogTrace(wxTRACE_FSWATCHER,
                   "'%s' is now watched %d times", canonical, count);

        wxUnusedVar(count); // could be unused if debug tracing is disabled
    }
    return true;
}

bool wxFileSystemWatcherBase::Remove(const wxFileName& path)
{
    // args validation & consistency checks
    wxString canonical = GetCanonicalPath(path);
    if (canonical.IsEmpty())
        return false;

    wxFSWatchInfoMap::iterator it = m_watches.find(canonical);
    wxCHECK_MSG(it != m_watches.end(), false,
                wxString::Format("Path '%s' is not watched", canonical));

    // Decrement the watch's refcount and remove from watch-list if 0
    bool ret = true;
    wxFSWatchInfo& watch = it->second;
    if ( !watch.DecRef() )
    {
        // remove in a platform specific way
        ret = m_service->Remove(watch);

        m_watches.erase(it);
    }
    return ret;
}

bool wxFileSystemWatcherBase::AddTree(const wxFileName& path, int events,
                                      const wxString& filespec)
{
    if (!path.DirExists())
        return false;

    // OPT could be optimised if we stored information about relationships
    // between paths
    class AddTraverser : public wxDirTraverser
    {
    public:
        AddTraverser(wxFileSystemWatcherBase* watcher, int events,
                     const wxString& filespec) :
            m_watcher(watcher), m_events(events), m_filespec(filespec)
        {
        }

        virtual wxDirTraverseResult OnFile(const wxString& WXUNUSED(filename))
        {
            // There is no need to watch individual files as we watch the
            // parent directory which will notify us about any changes in them.
            return wxDIR_CONTINUE;
        }

        virtual wxDirTraverseResult OnDir(const wxString& dirname)
        {
            if ( m_watcher->AddAny(wxFileName::DirName(dirname),
                                   m_events, wxFSWPath_Tree, m_filespec) )
            {
                wxLogTrace(wxTRACE_FSWATCHER,
                   "--- AddTree adding directory '%s' ---", dirname);
            }
            return wxDIR_CONTINUE;
        }

    private:
        wxFileSystemWatcherBase* m_watcher;
        int m_events;
        wxString m_filespec;
    };

    wxDir dir(path.GetFullPath());
    // Prevent asserts or infinite loops in trees containing symlinks
    int flags = wxDIR_DIRS;
    if ( !path.ShouldFollowLink() )
    {
        flags |= wxDIR_NO_FOLLOW;
    }
    AddTraverser traverser(this, events, filespec);
    dir.Traverse(traverser, filespec, flags);

    // Add the path itself explicitly as Traverse() doesn't return it.
    AddAny(path.GetPathWithSep(), events, wxFSWPath_Tree, filespec);

    return true;
}

bool wxFileSystemWatcherBase::RemoveTree(const wxFileName& path)
{
    if (!path.DirExists())
        return false;

    // OPT could be optimised if we stored information about relationships
    // between paths
    class RemoveTraverser : public wxDirTraverser
    {
    public:
        RemoveTraverser(wxFileSystemWatcherBase* watcher,
                        const wxString& filespec) :
            m_watcher(watcher), m_filespec(filespec)
        {
        }

        virtual wxDirTraverseResult OnFile(const wxString& WXUNUSED(filename))
        {
            // We never watch the individual files when watching the tree, so
            // nothing to do here.
            return wxDIR_CONTINUE;
        }

        virtual wxDirTraverseResult OnDir(const wxString& dirname)
        {
            m_watcher->Remove(wxFileName::DirName(dirname));
            return wxDIR_CONTINUE;
        }

    private:
        wxFileSystemWatcherBase* m_watcher;
        wxString m_filespec;
    };

    // If AddTree() used a filespec, we must use the same one
    wxString canonical = GetCanonicalPath(path);
    wxFSWatchInfoMap::iterator it = m_watches.find(canonical);
    wxCHECK_MSG( it != m_watches.end(), false,
                 wxString::Format("Path '%s' is not watched", canonical) );
    wxFSWatchInfo watch = it->second;
    const wxString filespec = watch.GetFilespec();

#if defined(__WINDOWS__)
    // When there's no filespec, the wxMSW AddTree() would have set a watch
    // on only the passed 'path'. We must therefore remove only this
    if (filespec.empty())
    {
        return Remove(path);
    }
    // Otherwise fall through to the generic implementation
#endif // __WINDOWS__

    wxDir dir(path.GetFullPath());
    // AddTree() might have used the wxDIR_NO_FOLLOW to prevent asserts or
    // infinite loops in trees containing symlinks. We need to do the same
    // or we'll try to remove unwatched items. Let's hope the caller used
    // the same ShouldFollowLink() setting as in AddTree()...
    int flags = wxDIR_DIRS;
    if ( !path.ShouldFollowLink() )
    {
        flags |= wxDIR_NO_FOLLOW;
    }
    RemoveTraverser traverser(this, filespec);
    dir.Traverse(traverser, filespec, flags);

    // As in AddTree() above, handle the path itself explicitly.
    Remove(path);

    return true;
}

bool wxFileSystemWatcherBase::RemoveAll()
{
    const bool ret = m_service->RemoveAll();
    m_watches.clear();
    return ret;
}

int wxFileSystemWatcherBase::GetWatchedPathsCount() const
{
    return m_watches.size();
}

int wxFileSystemWatcherBase::GetWatchedPaths(wxArrayString* paths) const
{
    wxCHECK_MSG( paths != NULL, -1, "Null array passed to retrieve paths");

    wxFSWatchInfoMap::const_iterator it = m_watches.begin();
    for ( ; it != m_watches.end(); ++it)
    {
        paths->push_back(it->first);
    }

    return m_watches.size();
}

#endif // wxUSE_FSWATCHER
