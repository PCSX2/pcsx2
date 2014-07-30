/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/fswatcher_inotify.h
// Purpose:     File system watcher impl classes
// Author:      Bartosz Bekier
// Created:     2009-05-26
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef WX_UNIX_PRIVATE_FSWATCHER_INOTIFY_H_
#define WX_UNIX_PRIVATE_FSWATCHER_INOTIFY_H_

#include "wx/filename.h"
#include "wx/evtloopsrc.h"

// ============================================================================
// wxFSWatcherEntry implementation & helper declarations
// ============================================================================

class wxFSWatcherImplUNIX;

class wxFSWatchEntry : public wxFSWatchInfo
{
public:
    wxFSWatchEntry(const wxFSWatchInfo& winfo) :
        wxFSWatchInfo(winfo)
    {
    }

    int GetWatchDescriptor() const
    {
        return m_wd;
    }

    void SetWatchDescriptor(int wd)
    {
        m_wd = wd;
    }

private:
    int m_wd;

    wxDECLARE_NO_COPY_CLASS(wxFSWatchEntry);
};


// ============================================================================
// wxFSWSourceHandler helper class
// ============================================================================

class wxFSWatcherImplUnix;

/**
 * Handler for handling i/o from inotify descriptor
 */
class wxFSWSourceHandler : public wxEventLoopSourceHandler
{
public:
    wxFSWSourceHandler(wxFSWatcherImplUnix* service) :
        m_service(service)
    {  }

    virtual void OnReadWaiting();
    virtual void OnWriteWaiting();
    virtual void OnExceptionWaiting();

protected:
    wxFSWatcherImplUnix* m_service;
};

#endif /* WX_UNIX_PRIVATE_FSWATCHER_INOTIFY_H_ */
