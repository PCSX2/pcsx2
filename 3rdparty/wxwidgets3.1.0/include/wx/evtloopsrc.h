///////////////////////////////////////////////////////////////////////////////
// Name:        wx/evtloopsrc.h
// Purpose:     declaration of wxEventLoopSource class
// Author:      Vadim Zeitlin
// Created:     2009-10-21
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EVTLOOPSRC_H_
#define _WX_EVTLOOPSRC_H_

// Include the header to get wxUSE_EVENTLOOP_SOURCE definition from it.
#include "wx/evtloop.h"
// ----------------------------------------------------------------------------
// wxEventLoopSource: a source of events which may be added to wxEventLoop
// ----------------------------------------------------------------------------

// TODO: refactor wxSocket under Unix to reuse wxEventLoopSource instead of
//       duplicating much of its logic
//
// TODO: freeze the API and document it

#if wxUSE_EVENTLOOP_SOURCE

#define wxTRACE_EVT_SOURCE "EventSource"

// handler used to process events on event loop sources
class wxEventLoopSourceHandler
{
public:
    // called when descriptor is available for non-blocking read
    virtual void OnReadWaiting() = 0;

    // called when descriptor is available  for non-blocking write
    virtual void OnWriteWaiting() = 0;

    // called when there is exception on descriptor
    virtual void OnExceptionWaiting() = 0;

    // virtual dtor for the base class
    virtual ~wxEventLoopSourceHandler() { }
};

// flags describing which kind of IO events we're interested in
enum
{
    wxEVENT_SOURCE_INPUT = 0x01,
    wxEVENT_SOURCE_OUTPUT = 0x02,
    wxEVENT_SOURCE_EXCEPTION = 0x04,
    wxEVENT_SOURCE_ALL = wxEVENT_SOURCE_INPUT |
                         wxEVENT_SOURCE_OUTPUT |
                         wxEVENT_SOURCE_EXCEPTION
};

// wxEventLoopSource itself is an ABC and can't be created directly, currently
// the only way to create it is by using wxEventLoop::AddSourceForFD().
class wxEventLoopSource
{
public:
    // dtor is pure virtual because it must be overridden to remove the source
    // from the event loop monitoring it
    virtual ~wxEventLoopSource() = 0;

    void SetHandler(wxEventLoopSourceHandler* handler) { m_handler = handler; }
    wxEventLoopSourceHandler* GetHandler() const { return m_handler; }

    void SetFlags(int flags) { m_flags = flags; }
    int GetFlags() const { return m_flags; }

protected:
    // ctor is only used by the derived classes
    wxEventLoopSource(wxEventLoopSourceHandler *handler, int flags)
        : m_handler(handler),
          m_flags(flags)
    {
    }

    wxEventLoopSourceHandler* m_handler;
    int m_flags;

    wxDECLARE_NO_COPY_CLASS(wxEventLoopSource);
};

inline wxEventLoopSource::~wxEventLoopSource() { }

#if defined(__UNIX__)
    #include "wx/unix/evtloopsrc.h"
#endif // __UNIX__

#if defined(__WXGTK20__)
    #include "wx/gtk/evtloopsrc.h"
#endif

#if defined(__DARWIN__)
    #include "wx/osx/evtloopsrc.h"
#elif defined(__WXQT__)
     #include "wx/unix/evtloopsrc.h"
#endif

#endif // wxUSE_EVENTLOOP_SOURCE

#endif // _WX_EVTLOOPSRC_H_

