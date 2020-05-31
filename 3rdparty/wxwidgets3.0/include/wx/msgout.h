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

// NB: VC6 has a bug that causes linker errors if you have template methods
//     in a class using __declspec(dllimport). The solution is to split such
//     class into two classes, one that contains the template methods and does
//     *not* use WXDLLIMPEXP_BASE and another class that contains the rest
//     (with DLL linkage).
class wxMessageOutputBase
{
public:
    virtual ~wxMessageOutputBase() { }

    // show a message to the user
    // void Printf(const wxString& format, ...) = 0;
    WX_DEFINE_VARARG_FUNC_VOID(Printf, 1, (const wxFormatString&),
                               DoPrintfWchar, DoPrintfUtf8)
#ifdef __WATCOMC__
    // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
    WX_VARARG_WATCOM_WORKAROUND(void, Printf, 1, (const wxString&),
                                (wxFormatString(f1)));
    WX_VARARG_WATCOM_WORKAROUND(void, Printf, 1, (const wxCStrData&),
                                (wxFormatString(f1)));
    WX_VARARG_WATCOM_WORKAROUND(void, Printf, 1, (const char*),
                                (wxFormatString(f1)));
    WX_VARARG_WATCOM_WORKAROUND(void, Printf, 1, (const wchar_t*),
                                (wxFormatString(f1)));
#endif

    // called by DoPrintf() to output formatted string but can also be called
    // directly if no formatting is needed
    virtual void Output(const wxString& str) = 0;

protected:
    // NB: this is pure virtual so that it can be implemented in dllexported
    //     wxMessagOutput class
#if !wxUSE_UTF8_LOCALE_ONLY
    virtual void DoPrintfWchar(const wxChar *format, ...) = 0;
#endif
#if wxUSE_UNICODE_UTF8
    virtual void DoPrintfUtf8(const char *format, ...) = 0;
#endif
};

#ifdef __VISUALC__
    // "non dll-interface class 'wxStringPrintfMixin' used as base interface
    // for dll-interface class 'wxString'" -- this is OK in our case
    #pragma warning (push)
    #pragma warning (disable:4275)
#endif

class WXDLLIMPEXP_BASE wxMessageOutput : public wxMessageOutputBase
{
public:
    virtual ~wxMessageOutput() { }

    // gets the current wxMessageOutput object (may be NULL during
    // initialization or shutdown)
    static wxMessageOutput* Get();

    // sets the global wxMessageOutput instance; returns the previous one
    static wxMessageOutput* Set(wxMessageOutput* msgout);

protected:
#if !wxUSE_UTF8_LOCALE_ONLY
    virtual void DoPrintfWchar(const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
    virtual void DoPrintfUtf8(const char *format, ...);
#endif

private:
    static wxMessageOutput* ms_msgOut;
};

#ifdef __VISUALC__
    #pragma warning (pop)
#endif

// ----------------------------------------------------------------------------
// implementation which sends output to stderr or specified file
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutputStderr : public wxMessageOutput
{
public:
    wxMessageOutputStderr(FILE *fp = stderr) : m_fp(fp) { }

    virtual void Output(const wxString& str);

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

    virtual void Output(const wxString& str);

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

    virtual void Output(const wxString& str);
};

#endif // wxUSE_GUI && wxUSE_MSGDLG

// ----------------------------------------------------------------------------
// implementation using the native way of outputting debug messages
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutputDebug : public wxMessageOutputStderr
{
public:
    wxMessageOutputDebug() { }

    virtual void Output(const wxString& str);
};

// ----------------------------------------------------------------------------
// implementation using wxLog (mainly for backwards compatibility)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMessageOutputLog : public wxMessageOutput
{
public:
    wxMessageOutputLog() { }

    virtual void Output(const wxString& str);
};

#endif // _WX_MSGOUT_H_
