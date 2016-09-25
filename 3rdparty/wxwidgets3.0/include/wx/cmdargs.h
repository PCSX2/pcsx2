///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cmdargs.h
// Purpose:     declaration of wxCmdLineArgsArray helper class
// Author:      Vadim Zeitlin
// Created:     2007-11-12
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CMDARGS_H_
#define _WX_CMDARGS_H_

#include "wx/arrstr.h"

// ----------------------------------------------------------------------------
// wxCmdLineArgsArray: helper class used by wxApp::argv
// ----------------------------------------------------------------------------

#if wxUSE_UNICODE

// this class is used instead of either "char **" or "wchar_t **" (neither of
// which would be backwards compatible with all the existing code) for argv
// field of wxApp
//
// as it's used for compatibility, it tries to look as much as traditional
// (char **) argv as possible, in particular it provides implicit conversions
// to "char **" and also array-like operator[]
class WXDLLIMPEXP_BASE wxCmdLineArgsArray
{
public:
    wxCmdLineArgsArray() { m_argsA = NULL; m_argsW = NULL; }

    template <typename T>
    wxCmdLineArgsArray& operator=(T **argv)
    {
        FreeArgs();

        m_args.clear();

        if ( argv )
        {
            while ( *argv )
                m_args.push_back(*argv++);
        }

        return *this;
    }

    operator char**() const
    {
        if ( !m_argsA )
        {
            const size_t count = m_args.size();
            m_argsA = new char *[count + 1];
            for ( size_t n = 0; n < count; n++ )
                m_argsA[n] = wxStrdup(m_args[n].ToAscii());

            m_argsA[count] = NULL;
        }

        return m_argsA;
    }

    operator wchar_t**() const
    {
        if ( !m_argsW )
        {
            const size_t count = m_args.size();
            m_argsW = new wchar_t *[count + 1];
            for ( size_t n = 0; n < count; n++ )
                m_argsW[n] = wxStrdup(m_args[n].wc_str());

            m_argsW[count] = NULL;
        }

        return m_argsW;
    }

    // existing code does checks like "if ( argv )" and we want it to continue
    // to compile, so provide this conversion even if it is pretty dangerous
    operator bool() const
    {
        return !m_args.empty();
    }

    // and the same for "if ( !argv )" checks
    bool operator!() const
    {
        return m_args.empty();
    }

    wxString operator[](size_t n) const
    {
        return m_args[n];
    }

    // we must provide this overload for g++ 3.4 which can't choose between
    // our operator[](size_t) and ::operator[](char**, int) otherwise
    wxString operator[](int n) const
    {
        return m_args[n];
    }


    // convenience methods, i.e. not existing only for backwards compatibility

    // do we have any arguments at all?
    bool IsEmpty() const { return m_args.empty(); }

    // access the arguments as a convenient array of wxStrings
    const wxArrayString& GetArguments() const { return m_args; }

    ~wxCmdLineArgsArray()
    {
        FreeArgs();
    }

private:
    template <typename T>
    void Free(T **args)
    {
        if ( !args )
            return;

        const size_t count = m_args.size();
        for ( size_t n = 0; n < count; n++ )
            free(args[n]);

        delete [] args;
    }

    void FreeArgs()
    {
        Free(m_argsA);
        Free(m_argsW);
    }

    wxArrayString m_args;
    mutable char **m_argsA;
    mutable wchar_t **m_argsW;

    wxDECLARE_NO_COPY_CLASS(wxCmdLineArgsArray);
};

// provide global operator overload for compatibility with the existing code
// doing things like "if ( condition && argv )"
inline bool operator&&(bool cond, const wxCmdLineArgsArray& array)
{
    return cond && !array.IsEmpty();
}

#endif // wxUSE_UNICODE

#endif // _WX_CMDARGS_H_

