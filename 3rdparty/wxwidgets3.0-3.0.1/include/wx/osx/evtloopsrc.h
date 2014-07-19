///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/evtloopsrc.h
// Purpose:     wxCFEventLoopSource class
// Author:      Vadim Zeitlin
// Created:     2009-10-21
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_EVTLOOPSRC_H_
#define _WX_OSX_EVTLOOPSRC_H_

typedef struct __CFSocket* CFSocketRef;

// ----------------------------------------------------------------------------
// wxCFEventLoopSource: CoreFoundation-based wxEventLoopSource for OS X
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxCFEventLoopSource : public wxEventLoopSource
{
public:
    // Create a new source in uninitialized state, call InitSocketRef() later
    // to associate it with the socket it is going to use.
    wxCFEventLoopSource(wxEventLoopSourceHandler *handler, int flags)
        : wxEventLoopSource(handler, flags)
    {
        m_cfSocket = NULL;
    }

    // Finish initialization of the event loop source by providing the
    // associated socket. This object takes ownership of it and will release it.
    void InitSourceSocket(CFSocketRef cfSocket);

    // Destructor deletes the associated socket.
    virtual ~wxCFEventLoopSource();

private:
    CFSocketRef m_cfSocket;

    wxDECLARE_NO_COPY_CLASS(wxCFEventLoopSource);
};

#endif // _WX_OSX_EVTLOOPSRC_H_

