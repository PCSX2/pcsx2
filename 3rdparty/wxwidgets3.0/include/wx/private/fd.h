///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/fd.h
// Purpose:     private stuff for working with file descriptors
// Author:      Vadim Zeitlin
// Created:     2008-11-23 (moved from wx/unix/private.h)
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_FD_H_
#define _WX_PRIVATE_FD_H_

// standard Linux headers produce many warnings when used with icc so define
// our own replacements for FD_XXX macros
#if defined(__INTELC__) && defined(__LINUX__)
    inline void wxFD_ZERO(fd_set *fds)
    {
        #pragma warning(push)
        #pragma warning(disable:593)
        FD_ZERO(fds);
        #pragma warning(pop)
    }

    inline void wxFD_SET(int fd, fd_set *fds)
    {
        #pragma warning(push, 1)
        #pragma warning(disable:1469)
        FD_SET(fd, fds);
        #pragma warning(pop)
    }

    inline bool wxFD_ISSET(int fd, fd_set *fds)
    {
        #pragma warning(push, 1)
        #pragma warning(disable:1469)
        return FD_ISSET(fd, fds);
        #pragma warning(pop)
    }
    inline void wxFD_CLR(int fd, fd_set *fds)
    {
        #pragma warning(push, 1)
        #pragma warning(disable:1469)
        FD_CLR(fd, fds);
        #pragma warning(pop)
    }
#else // !__INTELC__
    #define wxFD_ZERO(fds) FD_ZERO(fds)
    #define wxFD_SET(fd, fds) FD_SET(fd, fds)
    #define wxFD_ISSET(fd, fds) FD_ISSET(fd, fds)
    #define wxFD_CLR(fd, fds) FD_CLR(fd, fds)
#endif // __INTELC__/!__INTELC__

#endif // _WX_PRIVATE_FD_H_
