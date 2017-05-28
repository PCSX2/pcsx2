///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msgout.h
// Purpose:     wxMessageOutput class. Shows a message to the user
// Author:      Mattia Barbon
// Modified by:
// Created:     17.07.02
// Copyright:   (c) Mattia Barbon
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSGOUT_H_
#define _WX_MSGOUT_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"
#include "wx/chartype.h"
#include "wx/strvararg.h"

// ----------------------------------------------------------------------------
// wxMessageOutput is a class abstracting formatted output target, i.e.
// something you can printf() to
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutput
{
public:
    virtual ~wxMessageOutput() { }

    // gets the current wxMessageOutput object (may be NULL during
    // initialization or shutdown)
    static wxMessageOutput* Get();

    // sets the global wxMessageOutput instance; returns the previous one
    static wxMessageOutput* Set(wxMessageOutput* msgout);

    // show a message to the user
    // void Printf(const wxString& format, ...) = 0;
    WX_DEFINE_VARARG_FUNC_VOID(Printf, 1, (const wxFormatString&),
                               DoPrintfWchar, DoPrintfUtf8)

    // called by DoPrintf() to output formatted string but can also be called
    // directly if no formatting is needed
    virtual void Output(const wxString& str) = 0;

protected:
#if !wxUSE_UTF8_LOCALE_ONLY
    void DoPrintfWchar(const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
    void DoPrintfUtf8(const char *format, ...);
#endif

private:
    static wxMessageOutput* ms_msgOut;
};

// ----------------------------------------------------------------------------
// implementation which sends output to stderr or specified file
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutputStderr : public wxMessageOutput
{
public:
    wxMessageOutputStderr(FILE *fp = stderr) : m_fp(fp) { }

    virtual void Output(const wxString& str) wxOVERRIDE;

protected:
    // return the string with "\n" appended if it doesn't already terminate
    // with it (in which case it's returned unchanged)
    wxString AppendLineFeedIfNeeded(const wxString& str);

    FILE *m_fp;
};

// ----------------------------------------------------------------------------
// implementation showing the message to the user in "best" possible way:
// uses stderr or message box if available according to the flag given to ctor.
// ----------------------------------------------------------------------------

enum wxMessageOutputFlags
{
    wxMSGOUT_PREFER_STDERR = 0, // use stderr if available (this is the default)
    wxMSGOUT_PREFER_MSGBOX = 1  // always use message box if available
};

class WXDLLIMPEXP_BASE wxMessageOutputBest : public wxMessageOutputStderr
{
public:
    wxMessageOutputBest(wxMessageOutputFlags flags = wxMSGOUT_PREFER_STDERR)
        : m_flags(flags) { }

    virtual void Output(const wxString& str) wxOVERRIDE;

private:
    wxMessageOutputFlags m_flags;
};

// ----------------------------------------------------------------------------
// implementation which shows output in a message box
// ----------------------------------------------------------------------------

#if wxUSE_GUI && wxUSE_MSGDLG

class WXDLLIMPEXP_CORE wxMessageOutputMessageBox : public wxMessageOutput
{
public:
    wxMessageOutputMessageBox() { }

    virtual void Output(const wxString& str) wxOVERRIDE;
};

#endif // wxUSE_GUI && wxUSE_MSGDLG

// ----------------------------------------------------------------------------
// implementation using the native way of outputting debug messages
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutputDebug : public wxMessageOutputStderr
{
public:
    wxMessageOutputDebug() { }

    virtual void Output(const wxString& str) wxOVERRIDE;
};

// ----------------------------------------------------------------------------
// implementation using wxLog (mainly for backwards compatibility)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutputLog : public wxMessageOutput
{
public:
    wxMessageOutputLog() { }

    virtual void Output(const wxString& str) wxOVERRIDE;
};

#endif // _WX_MSGOUT_H_
