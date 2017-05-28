/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/fswatcher_kqueue.h
// Purpose:     wxKqueueFileSystemWatcher
// Author:      Bartosz Bekier
// Created:     2009-05-26
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FSWATCHER_KQUEUE_H_
#define _WX_FSWATCHER_KQUEUE_H_

#include "wx/defs.h"

#if wxUSE_FSWATCHER

class WXDLLIMPEXP_BASE wxKqueueFileSystemWatcher :
        public wxFileSystemWatcherBase
{
public:
    wxKqueueFileSystemWatcher();

    wxKqueueFileSystemWatcher(const wxFileName& path,
                              int events = wxFSW_EVENT_ALL);

    virtual ~wxKqueueFileSystemWatcher();

protected:
    bool Init();
};

#endif

#endif /* _WX_FSWATCHER_OSX_H_ */
