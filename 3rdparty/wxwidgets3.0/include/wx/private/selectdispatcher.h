/////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/selectdispatcher.h
// Purpose:     wxSelectDispatcher class
// Authors:     Lukasz Michalski and Vadim Zeitlin
// Created:     December 2006
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_SELECTDISPATCHER_H_
#define _WX_PRIVATE_SELECTDISPATCHER_H_

#include "wx/defs.h"

#if wxUSE_SELECT_DISPATCHER

#if defined(HAVE_SYS_SELECT_H) || defined(__WATCOMC__)
    #include <sys/time.h>
    #include <sys/select.h>
#endif

#ifdef __WATCOMC__
    #include <types.h>
    #include <sys/ioctl.h>
    #include <tcpustd.h>
#else
    #include <sys/types.h>
#endif

#include "wx/private/fdiodispatcher.h"

// helper class storing all the select() fd sets
class WXDLLIMPEXP_BASE wxSelectSets
{
public:
    // ctor zeroes out all fd_sets
    wxSelectSets();

    // default copy ctor, assignment operator and dtor are ok


    // return true if fd appears in any of the sets
    bool HasFD(int fd) const;

    // add or remove FD to our sets depending on whether flags contains
    // wxFDIO_INPUT/OUTPUT/EXCEPTION bits
    bool SetFD(int fd, int flags);

    // same as SetFD() except it unsets the bits set in the flags for the given
    // fd
    bool ClearFD(int fd)
    {
        return SetFD(fd, 0);
    }


    // call select() with our sets: the other parameters are the same as for
    // select() itself
    int Select(int nfds, struct timeval *tv);

    // call the handler methods corresponding to the sets having this fd if it
    // is present in any set and return true if it is
    bool Handle(int fd, wxFDIOHandler& handler) const;

private:
    typedef void (wxFDIOHandler::*Callback)();

    // the FD sets indices
    enum
    {
        Read,
        Write,
        Except,
        Max
    };

    // the sets used with select()
    fd_set m_fds[Max];

    // the wxFDIO_XXX flags, functions and names (used for debug messages only)
    // corresponding to the FD sets above
    static int ms_flags[Max];
    static const char *ms_names[Max];
    static Callback ms_handlers[Max];
};

class WXDLLIMPEXP_BASE wxSelectDispatcher : public wxMappedFDIODispatcher
{
public:
    // default ctor
    wxSelectDispatcher() { m_maxFD = -1; }

    // implement pure virtual methods of the base class
    virtual bool RegisterFD(int fd, wxFDIOHandler *handler, int flags = wxFDIO_ALL);
    virtual bool ModifyFD(int fd, wxFDIOHandler *handler, int flags = wxFDIO_ALL);
    virtual bool UnregisterFD(int fd);
    virtual bool HasPending() const;
    virtual int Dispatch(int timeout = TIMEOUT_INFINITE);

private:
    // common part of RegisterFD() and ModifyFD()
    bool DoUpdateFDAndHandler(int fd, wxFDIOHandler *handler, int flags);

    // call the handlers for the fds present in the given sets, return the
    // number of handlers we called
    int ProcessSets(const wxSelectSets& sets);

    // helper of ProcessSets(): call the handler if its fd is in the set
    void DoProcessFD(int fd, const fd_set& fds, wxFDIOHandler *handler,
                     const char *name);

    // common part of HasPending() and Dispatch(): calls select() with the
    // specified timeout
    int DoSelect(wxSelectSets& sets, int timeout) const;


    // the select sets containing all the registered fds
    wxSelectSets m_sets;

    // the highest registered fd value or -1 if none
    int m_maxFD;
};

#endif // wxUSE_SELECT_DISPATCHER

#endif // _WX_PRIVATE_SOCKETEVTDISPATCH_H_
