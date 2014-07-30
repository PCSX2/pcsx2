/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/epolldispatcher.h
// Purpose:     wxEpollDispatcher class
// Authors:     Lukasz Michalski
// Created:     April 2007
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_EPOLLDISPATCHER_H_
#define _WX_PRIVATE_EPOLLDISPATCHER_H_

#include "wx/defs.h"

#ifdef wxUSE_EPOLL_DISPATCHER

#include "wx/private/fdiodispatcher.h"

struct epoll_event;

class WXDLLIMPEXP_BASE wxEpollDispatcher : public wxFDIODispatcher
{
public:
    // create a new instance of this class, can return NULL if
    // epoll() is not supported on this system
    //
    // the caller should delete the returned pointer
    static wxEpollDispatcher *Create();

    virtual ~wxEpollDispatcher();

    // implement base class pure virtual methods
    virtual bool RegisterFD(int fd, wxFDIOHandler* handler, int flags = wxFDIO_ALL);
    virtual bool ModifyFD(int fd, wxFDIOHandler* handler, int flags = wxFDIO_ALL);
    virtual bool UnregisterFD(int fd);
    virtual bool HasPending() const;
    virtual int Dispatch(int timeout = TIMEOUT_INFINITE);

private:
    // ctor is private, use Create()
    wxEpollDispatcher(int epollDescriptor);

    // common part of HasPending() and Dispatch(): calls epoll_wait() with the
    // given timeout
    int DoPoll(epoll_event *events, int numEvents, int timeout) const;


    int m_epollDescriptor;
};

#endif // wxUSE_EPOLL_DISPATCHER

#endif // _WX_PRIVATE_SOCKETEVTDISPATCH_H_
