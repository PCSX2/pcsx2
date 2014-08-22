/*
 * Name:        wx/unix/chkconf.h
 * Purpose:     Unix-specific config settings consistency checks
 * Author:      Vadim Zeitlin
 * Created:     2007-07-14
 * Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#if wxUSE_CONSOLE_EVENTLOOP
#   if !wxUSE_SELECT_DISPATCHER && !wxUSE_EPOLL_DISPATCHER
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxSelect/EpollDispatcher needed for console event loop"
#       else
#           undef wxUSE_SELECT_DISPATCHER
#           define wxUSE_SELECT_DISPATCHER 1
#       endif
#   endif
#endif /* wxUSE_CONSOLE_EVENTLOOP */

#if wxUSE_FSWATCHER
#   if !defined(wxHAS_INOTIFY) && !defined(wxHAS_KQUEUE)
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxFileSystemWatcher requires either inotify() or kqueue()"
#       else
#           undef wxUSE_FSWATCHER
#           define wxUSE_FSWATCHER 0
#       endif
#   endif
#endif /* wxUSE_FSWATCHER */

#if wxUSE_GSTREAMER
#   if !wxUSE_THREADS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "GStreamer requires threads"
#       else
#           undef wxUSE_GSTREAMER
#           define wxUSE_GSTREAMER 0
#       endif
#   endif
#endif /* wxUSE_GSTREAMER */
