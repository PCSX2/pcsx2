///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/fdioeventloopsourcehandler.h
// Purpose:     declares wxFDIOEventLoopSourceHandler class
// Author:      Rob Bresalier, Vadim Zeitlin
// Created:     2013-06-13 (extracted from src/unix/evtloopunix.cpp)
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
//              (c) 2013 Rob Bresalier
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_FDIO_EVENT_LOOP_SOURCE_HANDLER_H
#define _WX_PRIVATE_FDIO_EVENT_LOOP_SOURCE_HANDLER_H

#include "wx/evtloopsrc.h"

// This class is a temporary bridge between event loop sources and
// FDIODispatcher. It is going to be removed soon, when all subject interfaces
// are modified
class wxFDIOEventLoopSourceHandler : public wxFDIOHandler
{
public:
    wxEXPLICIT wxFDIOEventLoopSourceHandler(wxEventLoopSourceHandler* handler)
        : m_handler(handler)
    {
    }

    // Just forward to the real handler.
    virtual void OnReadWaiting() { m_handler->OnReadWaiting(); }
    virtual void OnWriteWaiting() { m_handler->OnWriteWaiting(); }
    virtual void OnExceptionWaiting() { m_handler->OnExceptionWaiting(); }

protected:
    wxEventLoopSourceHandler* const m_handler;

    wxDECLARE_NO_COPY_CLASS(wxFDIOEventLoopSourceHandler);
};

#endif // _WX_PRIVATE_FDIO_EVENT_LOOP_SOURCE_HANDLER_H
