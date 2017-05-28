///////////////////////////////////////////////////////////////////////////////
// Name:        wx/protocol/log.h
// Purpose:     wxProtocolLog class for logging network exchanges
// Author:      Troelsk, Vadim Zeitlin
// Created:     2009-03-06
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PROTOCOL_LOG_H_
#define _WX_PROTOCOL_LOG_H_

#include "wx/string.h"

// ----------------------------------------------------------------------------
// wxProtocolLog: simple class for logging network requests and responses
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_NET wxProtocolLog
{
public:
    // Create object doing the logging using wxLogTrace() with the specified
    // trace mask.
    wxProtocolLog(const wxString& traceMask)
        : m_traceMask(traceMask)
    {
    }

    // Virtual dtor for the base class
    virtual ~wxProtocolLog() { }

    // Called by wxProtocol-derived classes to actually log something
    virtual void LogRequest(const wxString& str)
    {
        DoLogString("==> " + str);
    }

    virtual void LogResponse(const wxString& str)
    {
        DoLogString("<== " + str);
    }

protected:
    // Can be overridden by the derived classes.
    virtual void DoLogString(const wxString& str);

private:
    const wxString m_traceMask;

    wxDECLARE_NO_COPY_CLASS(wxProtocolLog);
};

#endif // _WX_PROTOCOL_LOG_H_

