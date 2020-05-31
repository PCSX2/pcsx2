/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/fswatcher.h
// Purpose:     wxMSWFileSystemWatcher
// Author:      Bartosz Bekier
// Created:     2009-05-26
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FSWATCHER_MSW_H_
#define _WX_FSWATCHER_MSW_H_

#include "wx/defs.h"

#if wxUSE_FSWATCHER

class WXDLLIMPEXP_BASE wxMSWFileSystemWatcher : public wxFileSystemWatcherBase
{
public:
    wxMSWFileSystemWatcher();

    wxMSWFileSystemWatcher(const wxFileName& path,
                           int events = wxFSW_EVENT_ALL);

    // Override the base class function to provide a much more efficient
    // implementation for it using the platform native support for watching the
    // entire directory trees.
    virtual bool AddTree(const wxFileName& path, int events = wxFSW_EVENT_ALL,
                         const wxString& filter = wxEmptyString);

protected:
    bool Init();
};

#endif // wxUSE_FSWATCHER

#endif /* _WX_FSWATCHER_MSW_H_ */
