///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/stdpaths.cpp
// Purpose:     wxStandardPaths implementation for Win32
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-10-19
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STDPATHS

#include "wx/stdpaths.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/dynlib.h"
#include "wx/filename.h"

#include "wx/msw/private.h"
#include "wx/msw/wrapshl.h"

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

typedef HRESULT (WINAPI *SHGetFolderPath_t)(HWND, int, HANDLE, DWORD, LPTSTR);
typedef HRESULT (WINAPI *SHGetSpecialFolderPath_t)(HWND, LPTSTR, int, BOOL);

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// used in our wxLogTrace messages
#define TRACE_MASK wxT("stdpaths")

#ifndef CSIDL_APPDATA
    #define CSIDL_APPDATA         0x001a
#endif

#ifndef CSIDL_LOCAL_APPDATA
    #define CSIDL_LOCAL_APPDATA   0x001c
#endif

#ifndef CSIDL_COMMON_APPDATA
    #define CSIDL_COMMON_APPDATA  0x0023
#endif

#ifndef CSIDL_PROGRAM_FILES
    #define CSIDL_PROGRAM_FILES   0x0026
#endif

#ifndef CSIDL_PERSONAL
    #define CSIDL_PERSONAL        0x0005
#endif

#ifndef SHGFP_TYPE_CURRENT
    #define SHGFP_TYPE_CURRENT 0
#endif

#ifndef SHGFP_TYPE_DEFAULT
    #define SHGFP_TYPE_DEFAULT 1
#endif

// ----------------------------------------------------------------------------
// module globals
// ----------------------------------------------------------------------------

namespace
{

struct ShellFunctions
{
    ShellFunctions()
    {
        pSHGetFolderPath = NULL;
        pSHGetSpecialFolderPath = NULL;
        initialized = false;
    }

    SHGetFolderPath_t pSHGetFolderPath;
    SHGetSpecialFolderPath_t pSHGetSpecialFolderPath;

    bool initialized;
};

// in spite of using a static variable, this is MT-safe as in the worst case it
// results in initializing the function pointer several times -- but this is
// harmless
ShellFunctions gs_shellFuncs;

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

void ResolveShellFunctions()
{
#if wxUSE_DYNLIB_CLASS

    // start with the newest functions, fall back to the oldest ones
#ifdef __WXWINCE__
    wxString shellDllName(wxT("coredll"));
#else
    // first check for SHGetFolderPath (shell32.dll 5.0)
    wxString shellDllName(wxT("shell32"));
#endif

    wxDynamicLibrary dllShellFunctions( shellDllName );
    if ( !dllShellFunctions.IsLoaded() )
    {
        wxLogTrace(TRACE_MASK, wxT("Failed to load %s.dll"), shellDllName.c_str() );
    }

    // don't give errors if the functions are unavailable, we're ready to deal
    // with this
    wxLogNull noLog;

#if wxUSE_UNICODE
    #ifdef __WXWINCE__
        static const wchar_t UNICODE_SUFFIX = L''; // WinCE SH functions don't seem to have 'W'
    #else
        static const wchar_t UNICODE_SUFFIX = L'W';
    #endif
#else // !Unicode
    static const char UNICODE_SUFFIX = 'A';
#endif // Unicode/!Unicode

    wxString funcname(wxT("SHGetFolderPath"));
    gs_shellFuncs.pSHGetFolderPath =
        (SHGetFolderPath_t)dllShellFunctions.GetSymbol(funcname + UNICODE_SUFFIX);

    // then for SHGetSpecialFolderPath (shell32.dll 4.71)
    if ( !gs_shellFuncs.pSHGetFolderPath )
    {
        funcname = wxT("SHGetSpecialFolderPath");
        gs_shellFuncs.pSHGetSpecialFolderPath = (SHGetSpecialFolderPath_t)
            dllShellFunctions.GetSymbol(funcname + UNICODE_SUFFIX);
    }

    // finally we fall back on SHGetSpecialFolderLocation (shell32.dll 4.0),
    // but we don't need to test for it -- it is available even under Win95

    // shell32.dll is going to be unloaded, but it still remains in memory
    // because we also link to it statically, so it's ok

    gs_shellFuncs.initialized = true;
#endif
}

} // anonymous namespace

// ============================================================================
// wxStandardPaths implementation
// ============================================================================

// ----------------------------------------------------------------------------
// private helpers
// ----------------------------------------------------------------------------

/* static */
wxString wxStandardPaths::DoGetDirectory(int csidl)
{
    if ( !gs_shellFuncs.initialized )
        ResolveShellFunctions();

    wxString dir;
    HRESULT hr = E_FAIL;

    // test whether the function is available during compile-time (it must be
    // defined as either "SHGetFolderPathA" or "SHGetFolderPathW")
#ifdef SHGetFolderPath
    // and now test whether we have it during run-time
    if ( gs_shellFuncs.pSHGetFolderPath )
    {
        hr = gs_shellFuncs.pSHGetFolderPath
             (
                NULL,               // parent window, not used
                csidl,
                NULL,               // access token (current user)
                SHGFP_TYPE_CURRENT, // current path, not just default value
                wxStringBuffer(dir, MAX_PATH)
             );

        // somewhat incredibly, the error code in the Unicode version is
        // different from the one in ASCII version for this function
#if wxUSE_UNICODE
        if ( hr == E_FAIL )
#else
        if ( hr == S_FALSE )
#endif
        {
            // directory doesn't exist, maybe we can get its default value?
            hr = gs_shellFuncs.pSHGetFolderPath
                 (
                    NULL,
                    csidl,
                    NULL,
                    SHGFP_TYPE_DEFAULT,
                    wxStringBuffer(dir, MAX_PATH)
                 );
        }
    }
#endif // SHGetFolderPath

#ifdef SHGetSpecialFolderPath
    if ( FAILED(hr) && gs_shellFuncs.pSHGetSpecialFolderPath )
    {
        hr = gs_shellFuncs.pSHGetSpecialFolderPath
             (
                NULL,               // parent window
                wxStringBuffer(dir, MAX_PATH),
                csidl,
                FALSE               // don't create if doesn't exist
             );
    }
#endif // SHGetSpecialFolderPath

    // SHGetSpecialFolderLocation should be available with all compilers and
    // under all Win32 systems, so don't test for it (and as it doesn't exist
    // in "A" and "W" versions anyhow, testing would be more involved, too)
    if ( FAILED(hr) )
    {
        LPITEMIDLIST pidl;
        hr = SHGetSpecialFolderLocation(NULL, csidl, &pidl);

        if ( SUCCEEDED(hr) )
        {
            // creating this temp object has (nice) side effect of freeing pidl
            dir = wxItemIdList(pidl).GetPath();
        }
    }

    return dir;
}

wxString wxStandardPaths::GetAppDir() const
{
    if ( m_appDir.empty() )
    {
        m_appDir = wxFileName(wxGetFullModuleName()).GetPath();
    }

    return m_appDir;
}

wxString wxStandardPaths::GetDocumentsDir() const
{
    return DoGetDirectory(CSIDL_PERSONAL);
}

// ----------------------------------------------------------------------------
// MSW-specific functions
// ----------------------------------------------------------------------------

void wxStandardPaths::IgnoreAppSubDir(const wxString& subdirPattern)
{
    wxFileName fn = wxFileName::DirName(GetAppDir());

    if ( !fn.GetDirCount() )
    {
        // no last directory to ignore anyhow
        return;
    }

    const wxString lastdir = fn.GetDirs().Last().Lower();
    if ( lastdir.Matches(subdirPattern.Lower()) )
    {
        fn.RemoveLastDir();

        // store the cached value so that subsequent calls to GetAppDir() will
        // reuse it instead of using just the program binary directory
        m_appDir = fn.GetPath();
    }
}

void wxStandardPaths::IgnoreAppBuildSubDirs()
{
    IgnoreAppSubDir("debug");
    IgnoreAppSubDir("release");

    wxString compilerPrefix;
#ifdef __VISUALC__
    compilerPrefix = "vc";
#elif defined(__GNUG__)
    compilerPrefix = "gcc";
#elif defined(__BORLANDC__)
    compilerPrefix = "bcc";
#elif defined(__DIGITALMARS__)
    compilerPrefix = "dmc";
#elif defined(__WATCOMC__)
    compilerPrefix = "wat";
#else
    return;
#endif

    IgnoreAppSubDir(compilerPrefix + "_msw*");
}

void wxStandardPaths::DontIgnoreAppSubDir()
{
    // this will force the next call to GetAppDir() to use the program binary
    // path as the application directory
    m_appDir.clear();
}

/* static */
wxString wxStandardPaths::MSWGetShellDir(int csidl)
{
    return DoGetDirectory(csidl);
}

// ----------------------------------------------------------------------------
// public functions
// ----------------------------------------------------------------------------

wxStandardPaths::wxStandardPaths()
{
    // make it possible to run uninstalled application from the build directory
    IgnoreAppBuildSubDirs();
}

wxString wxStandardPaths::GetExecutablePath() const
{
    return wxGetFullModuleName();
}

wxString wxStandardPaths::GetConfigDir() const
{
    return AppendAppInfo(DoGetDirectory(CSIDL_COMMON_APPDATA));
}

wxString wxStandardPaths::GetUserConfigDir() const
{
    return DoGetDirectory(CSIDL_APPDATA);
}

wxString wxStandardPaths::GetDataDir() const
{
    // under Windows each program is usually installed in its own directory and
    // so its datafiles are in the same directory as its main executable
    return GetAppDir();
}

wxString wxStandardPaths::GetUserDataDir() const
{
    return AppendAppInfo(GetUserConfigDir());
}

wxString wxStandardPaths::GetUserLocalDataDir() const
{
    return AppendAppInfo(DoGetDirectory(CSIDL_LOCAL_APPDATA));
}

wxString wxStandardPaths::GetPluginsDir() const
{
    // there is no standard location for plugins, suppose they're in the same
    // directory as the .exe
    return GetAppDir();
}

// ============================================================================
// wxStandardPathsWin16 implementation
// ============================================================================

wxString wxStandardPathsWin16::GetConfigDir() const
{
    // this is for compatibility with earlier wxFileConfig versions
    // which used the Windows directory for the global files
    wxString dir;
#ifndef __WXWINCE__
    if ( !::GetWindowsDirectory(wxStringBuffer(dir, MAX_PATH), MAX_PATH) )
    {
        wxLogLastError(wxT("GetWindowsDirectory"));
    }
#else
    // TODO: use CSIDL_WINDOWS (eVC4, possibly not eVC3)
    dir = wxT("\\Windows");
#endif

    return dir;
}

wxString wxStandardPathsWin16::GetUserConfigDir() const
{
    // again, for wxFileConfig which uses $HOME for its user config file
    return wxGetHomeDir();
}

#endif // wxUSE_STDPATHS
