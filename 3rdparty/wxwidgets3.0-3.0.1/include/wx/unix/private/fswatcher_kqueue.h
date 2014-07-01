/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/fswatcher_kqueue.h
// Purpose:     File system watcher impl classes
// Author:      Bartosz Bekier
// Created:     2009-05-26
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef WX_UNIX_PRIVATE_FSWATCHER_KQUEUE_H_
#define WX_UNIX_PRIVATE_FSWATCHER_KQUEUE_H_

#include <fcntl.h>
#include <unistd.h>
#include "wx/dir.h"
#include "wx/debug.h"
#include "wx/arrstr.h"

// ============================================================================
// wxFSWatcherEntry implementation & helper declarations
// ============================================================================

class wxFSWatcherImplKqueue;

class wxFSWatchEntryKq : public wxFSWatchInfo
{
public:
    struct wxDirState
    {
        wxDirState(const wxFSWatchInfo& winfo)
        {
            if (!wxDir::Exists(winfo.GetPath()))
                return;

            wxDir dir(winfo.GetPath());
            wxCHECK_RET( dir.IsOpened(),
                  wxString::Format("Unable to open dir '%s'", winfo.GetPath()));

            wxString filename;
            bool ret = dir.GetFirst(&filename);
            while (ret)
            {
                files.push_back(filename);
                ret = dir.GetNext(&filename);
            }
        }

        wxSortedArrayString files;
    };

    wxFSWatchEntryKq(const wxFSWatchInfo& winfo) :
        wxFSWatchInfo(winfo), m_lastState(winfo)
    {
        m_fd = wxOpen(m_path, O_RDONLY, 0);
        if (m_fd == -1)
        {
            wxLogSysError(_("Unable to open path '%s'"), m_path);
        }
    }

    virtual ~wxFSWatchEntryKq()
    {
        (void) Close();
    }

    bool Close()
    {
        if (!IsOk())
            return false;

        int ret = close(m_fd);
        if (ret == -1)
        {
            wxLogSysError(_("Unable to close path '%s'"), m_path);
        }
        m_fd = -1;

        return ret != -1;
    }

    bool IsOk() const
    {
        return m_fd != -1;
    }

    int GetFileDescriptor() const
    {
        return m_fd;
    }

    void RefreshState()
    {
        m_lastState = wxDirState(*this);
    }

    const wxDirState& GetLastState() const
    {
        return m_lastState;
    }

private:
    int m_fd;
    wxDirState m_lastState;

    wxDECLARE_NO_COPY_CLASS(wxFSWatchEntryKq);
};

#endif /* WX_UNIX_PRIVATE_FSWATCHER_KQUEUE_H_ */
