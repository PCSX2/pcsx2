/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/utils.cpp
// Purpose:     Various utilities
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/log.h"
#endif  //WX_PRECOMP

#include "wx/msw/registry.h"
#include "wx/apptrait.h"
#include "wx/dynlib.h"
#include "wx/dynload.h"
#include "wx/scopeguard.h"
#include "wx/filename.h"

#include "wx/confbase.h"        // for wxExpandEnvVars()

#include "wx/msw/private.h"     // includes <windows.h>
#include "wx/msw/private/hiddenwin.h"
#include "wx/msw/missing.h"     // for CHARSET_HANGUL

#if defined(__CYGWIN__)
    //CYGWIN gives annoying warning about runtime stuff if we don't do this
#   define USE_SYS_TYPES_FD_SET
#   include <sys/types.h>
#endif

// Doesn't work with Cygwin at present
#if wxUSE_SOCKETS && (defined(__GNUWIN32_OLD__) || defined(__WXWINCE__) || defined(__CYGWIN32__))
    // apparently we need to include winsock.h to get WSADATA and other stuff
    // used in wxGetFullHostName() with the old mingw32 versions
    #include <winsock.h>
#endif

#if !defined(__GNUWIN32__) && !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    #include <direct.h>

    #include <dos.h>
#endif  //GNUWIN32

#if defined(__CYGWIN__)
    #include <sys/unistd.h>
    #include <sys/stat.h>
    #include <sys/cygwin.h> // for cygwin_conv_path()
    // and cygwin_conv_to_full_win32_path()
    #include <cygwin/version.h>
#endif  //GNUWIN32

#ifdef __BORLANDC__ // Please someone tell me which version of Borland needs
                    // this (3.1 I believe) and how to test for it.
                    // If this works for Borland 4.0 as well, then no worries.
    #include <dir.h>
#endif

// VZ: there is some code using NetXXX() functions to get the full user name:
//     I don't think it's a good idea because they don't work under Win95 and
//     seem to return the same as wxGetUserId() under NT. If you really want
//     to use them, just #define USE_NET_API
#undef USE_NET_API

#ifdef USE_NET_API
    #include <lm.h>
#endif // USE_NET_API

#if defined(__WIN32__) && !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    #ifndef __UNIX__
        #include <io.h>
    #endif

    #ifndef __GNUWIN32__
        #include <shellapi.h>
    #endif
#endif

#ifndef __WATCOMC__
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif

// For wxKillAllChildren
#include <tlhelp32.h>

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// In the WIN.INI file
#if (!defined(USE_NET_API) && !defined(__WXWINCE__)) || defined(__WXMICROWIN__)
static const wxChar WX_SECTION[] = wxT("wxWindows");
#endif

#if (!defined(USE_NET_API) && !defined(__WXWINCE__))
static const wxChar eUSERNAME[]  = wxT("UserName");
#endif

WXDLLIMPEXP_DATA_BASE(const wxChar *) wxUserResourceStr = wxT("TEXT");

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// get host name and related
// ----------------------------------------------------------------------------

// Get hostname only (without domain name)
bool wxGetHostName(wxChar *buf, int maxSize)
{
#if defined(__WXWINCE__)
    // GetComputerName() is not supported but the name seems to be stored in
    // this location in the registry, at least for PPC2003 and WM5
    wxString hostName;
    wxRegKey regKey(wxRegKey::HKLM, wxT("Ident"));
    if ( !regKey.HasValue(wxT("Name")) ||
            !regKey.QueryValue(wxT("Name"), hostName) )
        return false;

    wxStrlcpy(buf, hostName.t_str(), maxSize);
#else // !__WXWINCE__
    DWORD nSize = maxSize;
    if ( !::GetComputerName(buf, &nSize) )
    {
        wxLogLastError(wxT("GetComputerName"));

        return false;
    }
#endif // __WXWINCE__/!__WXWINCE__

    return true;
}

// get full hostname (with domain name if possible)
bool wxGetFullHostName(wxChar *buf, int maxSize)
{
#if !defined( __WXMICROWIN__) && wxUSE_DYNLIB_CLASS && wxUSE_SOCKETS
    // TODO should use GetComputerNameEx() when available

    // we don't want to always link with Winsock DLL as we might not use it at
    // all, so load it dynamically here if needed (and don't complain if it is
    // missing, we handle this)
    wxLogNull noLog;

    wxDynamicLibrary dllWinsock(wxT("ws2_32.dll"), wxDL_VERBATIM);
    if ( dllWinsock.IsLoaded() )
    {
        typedef int (PASCAL *WSAStartup_t)(WORD, WSADATA *);
        typedef int (PASCAL *gethostname_t)(char *, int);
        typedef hostent* (PASCAL *gethostbyname_t)(const char *);
        typedef hostent* (PASCAL *gethostbyaddr_t)(const char *, int , int);
        typedef int (PASCAL *WSACleanup_t)(void);

        #define LOAD_WINSOCK_FUNC(func)                                       \
            func ## _t                                                        \
                pfn ## func = (func ## _t)dllWinsock.GetSymbol(wxT(#func))

        LOAD_WINSOCK_FUNC(WSAStartup);

        WSADATA wsa;
        if ( pfnWSAStartup && pfnWSAStartup(MAKEWORD(1, 1), &wsa) == 0 )
        {
            LOAD_WINSOCK_FUNC(gethostname);

            wxString host;
            if ( pfngethostname )
            {
                char bufA[256];
                if ( pfngethostname(bufA, WXSIZEOF(bufA)) == 0 )
                {
                    // gethostname() won't usually include the DNS domain name,
                    // for this we need to work a bit more
                    if ( !strchr(bufA, '.') )
                    {
                        LOAD_WINSOCK_FUNC(gethostbyname);

                        struct hostent *pHostEnt = pfngethostbyname
                                                    ? pfngethostbyname(bufA)
                                                    : NULL;

                        if ( pHostEnt )
                        {
                            // Windows will use DNS internally now
                            LOAD_WINSOCK_FUNC(gethostbyaddr);

                            pHostEnt = pfngethostbyaddr
                                        ? pfngethostbyaddr(pHostEnt->h_addr,
                                                           4, AF_INET)
                                        : NULL;
                        }

                        if ( pHostEnt )
                        {
                            host = wxString::FromAscii(pHostEnt->h_name);
                        }
                    }
                }
            }

            LOAD_WINSOCK_FUNC(WSACleanup);
            if ( pfnWSACleanup )
                pfnWSACleanup();


            if ( !host.empty() )
            {
                wxStrlcpy(buf, host.c_str(), maxSize);

                return true;
            }
        }
    }
#endif // !__WXMICROWIN__

    return wxGetHostName(buf, maxSize);
}

// Get user ID e.g. jacs
bool wxGetUserId(wxChar *WXUNUSED_IN_WINCE(buf),
                 int WXUNUSED_IN_WINCE(maxSize))
{
#if defined(__WXWINCE__)
    // TODO-CE
    return false;
#else
    DWORD nSize = maxSize;
    if ( ::GetUserName(buf, &nSize) == 0 )
    {
        // actually, it does happen on Win9x if the user didn't log on
        DWORD res = ::GetEnvironmentVariable(wxT("username"), buf, maxSize);
        if ( res == 0 )
        {
            // not found
            return false;
        }
    }

    return true;
#endif
}

// Get user name e.g. Julian Smart
bool wxGetUserName(wxChar *buf, int maxSize)
{
    wxCHECK_MSG( buf && ( maxSize > 0 ), false,
                    wxT("empty buffer in wxGetUserName") );
#if defined(__WXWINCE__) && wxUSE_REGKEY
    wxLogNull noLog;
    wxRegKey key(wxRegKey::HKCU, wxT("ControlPanel\\Owner"));
    if(!key.Open(wxRegKey::Read))
        return false;
    wxString name;
    if(!key.QueryValue(wxT("Owner"),name))
        return false;
    wxStrlcpy(buf, name.c_str(), maxSize);
    return true;
#elif defined(USE_NET_API)
    CHAR szUserName[256];
    if ( !wxGetUserId(szUserName, WXSIZEOF(szUserName)) )
        return false;

    // TODO how to get the domain name?
    CHAR *szDomain = "";

    // the code is based on the MSDN example (also see KB article Q119670)
    WCHAR wszUserName[256];          // Unicode user name
    WCHAR wszDomain[256];
    LPBYTE ComputerName;

    USER_INFO_2 *ui2;         // User structure

    // Convert ANSI user name and domain to Unicode
    MultiByteToWideChar( CP_ACP, 0, szUserName, strlen(szUserName)+1,
            wszUserName, WXSIZEOF(wszUserName) );
    MultiByteToWideChar( CP_ACP, 0, szDomain, strlen(szDomain)+1,
            wszDomain, WXSIZEOF(wszDomain) );

    // Get the computer name of a DC for the domain.
    if ( NetGetDCName( NULL, wszDomain, &ComputerName ) != NERR_Success )
    {
        wxLogError(wxT("Cannot find domain controller"));

        goto error;
    }

    // Look up the user on the DC
    NET_API_STATUS status = NetUserGetInfo( (LPWSTR)ComputerName,
            (LPWSTR)&wszUserName,
            2, // level - we want USER_INFO_2
            (LPBYTE *) &ui2 );
    switch ( status )
    {
        case NERR_Success:
            // ok
            break;

        case NERR_InvalidComputer:
            wxLogError(wxT("Invalid domain controller name."));

            goto error;

        case NERR_UserNotFound:
            wxLogError(wxT("Invalid user name '%s'."), szUserName);

            goto error;

        default:
            wxLogSysError(wxT("Can't get information about user"));

            goto error;
    }

    // Convert the Unicode full name to ANSI
    WideCharToMultiByte( CP_ACP, 0, ui2->usri2_full_name, -1,
            buf, maxSize, NULL, NULL );

    return true;

error:
    wxLogError(wxT("Couldn't look up full user name."));

    return false;
#else  // !USE_NET_API
    // Could use NIS, MS-Mail or other site specific programs
    // Use wxWidgets configuration data
    bool ok = GetProfileString(WX_SECTION, eUSERNAME, wxEmptyString, buf, maxSize - 1) != 0;
    if ( !ok )
    {
        ok = wxGetUserId(buf, maxSize);
    }

    if ( !ok )
    {
        wxStrlcpy(buf, wxT("Unknown User"), maxSize);
    }

    return true;
#endif // Win32/16
}

const wxChar* wxGetHomeDir(wxString *pstr)
{
    wxString& strDir = *pstr;

    // first branch is for Cygwin
#if defined(__UNIX__) && !defined(__WINE__)
    const wxChar *szHome = wxGetenv(wxT("HOME"));
    if ( szHome == NULL ) {
      // we're homeless...
      wxLogWarning(_("can't find user's HOME, using current directory."));
      strDir = wxT(".");
    }
    else
       strDir = szHome;

    // add a trailing slash if needed
    if ( strDir.Last() != wxT('/') )
      strDir << wxT('/');

    #ifdef __CYGWIN__
        // Cygwin returns unix type path but that does not work well
        static wxChar windowsPath[MAX_PATH];
        #if CYGWIN_VERSION_DLL_MAJOR >= 1007
            cygwin_conv_path(CCP_POSIX_TO_WIN_W, strDir.c_str(), windowsPath, MAX_PATH);
        #else
            cygwin_conv_to_full_win32_path(strDir.c_str(), windowsPath);
        #endif
        strDir = windowsPath;
    #endif
#elif defined(__WXWINCE__)
    strDir = wxT("\\");
#else
    strDir.clear();

    // If we have a valid HOME directory, as is used on many machines that
    // have unix utilities on them, we should use that.
    const wxChar *szHome = wxGetenv(wxT("HOME"));

    if ( szHome != NULL )
    {
        strDir = szHome;
    }
    else // no HOME, try HOMEDRIVE/PATH
    {
        szHome = wxGetenv(wxT("HOMEDRIVE"));
        if ( szHome != NULL )
            strDir << szHome;
        szHome = wxGetenv(wxT("HOMEPATH"));

        if ( szHome != NULL )
        {
            strDir << szHome;

            // the idea is that under NT these variables have default values
            // of "%systemdrive%:" and "\\". As we don't want to create our
            // config files in the root directory of the system drive, we will
            // create it in our program's dir. However, if the user took care
            // to set HOMEPATH to something other than "\\", we suppose that he
            // knows what he is doing and use the supplied value.
            if ( wxStrcmp(szHome, wxT("\\")) == 0 )
                strDir.clear();
        }
    }

    if ( strDir.empty() )
    {
        // If we have a valid USERPROFILE directory, as is the case in
        // Windows NT, 2000 and XP, we should use that as our home directory.
        szHome = wxGetenv(wxT("USERPROFILE"));

        if ( szHome != NULL )
            strDir = szHome;
    }

    if ( !strDir.empty() )
    {
        // sometimes the value of HOME may be "%USERPROFILE%", so reexpand the
        // value once again, it shouldn't hurt anyhow
        strDir = wxExpandEnvVars(strDir);
    }
    else // fall back to the program directory
    {
        // extract the directory component of the program file name
        wxFileName::SplitPath(wxGetFullModuleName(), &strDir, NULL, NULL);
    }
#endif  // UNIX/Win

    return strDir.c_str();
}

wxString wxGetUserHome(const wxString& user)
{
    wxString home;

    if ( user.empty() || user == wxGetUserId() )
        wxGetHomeDir(&home);

    return home;
}

bool wxGetDiskSpace(const wxString& WXUNUSED_IN_WINCE(path),
                    wxDiskspaceSize_t *WXUNUSED_IN_WINCE(pTotal),
                    wxDiskspaceSize_t *WXUNUSED_IN_WINCE(pFree))
{
#ifdef __WXWINCE__
    // TODO-CE
    return false;
#else
    if ( path.empty() )
        return false;

// old w32api don't have ULARGE_INTEGER
#if defined(__WIN32__) && \
    (!defined(__GNUWIN32__) || wxCHECK_W32API_VERSION( 0, 3 ))
    // GetDiskFreeSpaceEx() is not available under original Win95, check for
    // it
    typedef BOOL (WINAPI *GetDiskFreeSpaceEx_t)(LPCTSTR,
                                                PULARGE_INTEGER,
                                                PULARGE_INTEGER,
                                                PULARGE_INTEGER);

    GetDiskFreeSpaceEx_t
        pGetDiskFreeSpaceEx = (GetDiskFreeSpaceEx_t)::GetProcAddress
                              (
                                ::GetModuleHandle(wxT("kernel32.dll")),
#if wxUSE_UNICODE
                                "GetDiskFreeSpaceExW"
#else
                                "GetDiskFreeSpaceExA"
#endif
                              );

    if ( pGetDiskFreeSpaceEx )
    {
        ULARGE_INTEGER bytesFree, bytesTotal;

        // may pass the path as is, GetDiskFreeSpaceEx() is smart enough
        if ( !pGetDiskFreeSpaceEx(path.t_str(),
                                  &bytesFree,
                                  &bytesTotal,
                                  NULL) )
        {
            wxLogLastError(wxT("GetDiskFreeSpaceEx"));

            return false;
        }

        // ULARGE_INTEGER is a union of a 64 bit value and a struct containing
        // two 32 bit fields which may be or may be not named - try to make it
        // compile in all cases
#if defined(__BORLANDC__) && !defined(_ANONYMOUS_STRUCT)
        #define UL(ul) ul.u
#else // anon union
        #define UL(ul) ul
#endif
        if ( pTotal )
        {
#if wxUSE_LONGLONG
            *pTotal = wxDiskspaceSize_t(UL(bytesTotal).HighPart, UL(bytesTotal).LowPart);
#else
            *pTotal = wxDiskspaceSize_t(UL(bytesTotal).LowPart);
#endif
        }

        if ( pFree )
        {
#if wxUSE_LONGLONG
            *pFree = wxLongLong(UL(bytesFree).HighPart, UL(bytesFree).LowPart);
#else
            *pFree = wxDiskspaceSize_t(UL(bytesFree).LowPart);
#endif
        }
    }
    else
#endif // Win32
    {
        // there's a problem with drives larger than 2GB, GetDiskFreeSpaceEx()
        // should be used instead - but if it's not available, fall back on
        // GetDiskFreeSpace() nevertheless...

        DWORD lSectorsPerCluster,
              lBytesPerSector,
              lNumberOfFreeClusters,
              lTotalNumberOfClusters;

        // FIXME: this is wrong, we should extract the root drive from path
        //        instead, but this is the job for wxFileName...
        if ( !::GetDiskFreeSpace(path.t_str(),
                                 &lSectorsPerCluster,
                                 &lBytesPerSector,
                                 &lNumberOfFreeClusters,
                                 &lTotalNumberOfClusters) )
        {
            wxLogLastError(wxT("GetDiskFreeSpace"));

            return false;
        }

        wxDiskspaceSize_t lBytesPerCluster = (wxDiskspaceSize_t) lSectorsPerCluster;
        lBytesPerCluster *= lBytesPerSector;

        if ( pTotal )
        {
            *pTotal = lBytesPerCluster;
            *pTotal *= lTotalNumberOfClusters;
        }

        if ( pFree )
        {
            *pFree = lBytesPerCluster;
            *pFree *= lNumberOfFreeClusters;
        }
    }

    return true;
#endif
    // __WXWINCE__
}

// ----------------------------------------------------------------------------
// env vars
// ----------------------------------------------------------------------------

bool wxGetEnv(const wxString& WXUNUSED_IN_WINCE(var),
              wxString *WXUNUSED_IN_WINCE(value))
{
#ifdef __WXWINCE__
    // no environment variables under CE
    return false;
#else // Win32
    // first get the size of the buffer
    DWORD dwRet = ::GetEnvironmentVariable(var.t_str(), NULL, 0);
    if ( !dwRet )
    {
        // this means that there is no such variable
        return false;
    }

    if ( value )
    {
        (void)::GetEnvironmentVariable(var.t_str(),
                                       wxStringBuffer(*value, dwRet),
                                       dwRet);
    }

    return true;
#endif // WinCE/32
}

bool wxDoSetEnv(const wxString& var, const wxChar *value)
{
#ifdef __WXWINCE__
    // no environment variables under CE
    wxUnusedVar(var);
    wxUnusedVar(value);
    return false;
#else // !__WXWINCE__
    // update the CRT environment if possible as people expect getenv() to also
    // work and it is not affected by Win32 SetEnvironmentVariable() call (OTOH
    // the CRT does use Win32 call to update the process environment block so
    // there is no need to call it)
    //
    // TODO: add checks for the other compilers (and update wxSetEnv()
    //       documentation in interface/wx/utils.h accordingly)
#if defined(__VISUALC__) || defined(__MINGW32__)
    // notice that Microsoft _putenv() has different semantics from POSIX
    // function with almost the same name: in particular it makes a copy of the
    // string instead of using it as part of environment so we can safely call
    // it here without going through all the troubles with wxSetEnvModule as in
    // src/unix/utilsunx.cpp
    wxString envstr = var;
    envstr += '=';
    if ( value )
        envstr += value;
    if ( _tputenv(envstr.t_str()) != 0 )
        return false;
#else // other compiler
    if ( !::SetEnvironmentVariable(var.t_str(), value) )
    {
        wxLogLastError(wxT("SetEnvironmentVariable"));

        return false;
    }
#endif // compiler

    return true;
#endif // __WXWINCE__/!__WXWINCE__
}

bool wxSetEnv(const wxString& variable, const wxString& value)
{
    return wxDoSetEnv(variable, value.t_str());
}

bool wxUnsetEnv(const wxString& variable)
{
    return wxDoSetEnv(variable, NULL);
}

// ----------------------------------------------------------------------------
// process management
// ----------------------------------------------------------------------------

// structure used to pass parameters from wxKill() to wxEnumFindByPidProc()
struct wxFindByPidParams
{
    wxFindByPidParams() { hwnd = 0; pid = 0; }

    // the HWND used to return the result
    HWND hwnd;

    // the PID we're looking from
    DWORD pid;

    wxDECLARE_NO_COPY_CLASS(wxFindByPidParams);
};

// wxKill helper: EnumWindows() callback which is used to find the first (top
// level) window belonging to the given process
BOOL CALLBACK wxEnumFindByPidProc(HWND hwnd, LPARAM lParam)
{
    DWORD pid;
    (void)::GetWindowThreadProcessId(hwnd, &pid);

    wxFindByPidParams *params = (wxFindByPidParams *)lParam;
    if ( pid == params->pid )
    {
        // remember the window we found
        params->hwnd = hwnd;

        // return FALSE to stop the enumeration
        return FALSE;
    }

    // continue enumeration
    return TRUE;
}

int wxKillAllChildren(long pid, wxSignal sig, wxKillError *krc);

int wxKill(long pid, wxSignal sig, wxKillError *krc, int flags)
{
    if (flags & wxKILL_CHILDREN)
        wxKillAllChildren(pid, sig, krc);

    // get the process handle to operate on
    DWORD dwAccess = PROCESS_QUERY_INFORMATION | SYNCHRONIZE;
    if ( sig == wxSIGKILL )
        dwAccess |= PROCESS_TERMINATE;

    HANDLE hProcess = ::OpenProcess(dwAccess, FALSE, (DWORD)pid);
    if ( hProcess == NULL )
    {
        if ( krc )
        {
            // recognize wxKILL_ACCESS_DENIED as special because this doesn't
            // mean that the process doesn't exist and this is important for
            // wxProcess::Exists()
            *krc = ::GetLastError() == ERROR_ACCESS_DENIED
                        ? wxKILL_ACCESS_DENIED
                        : wxKILL_NO_PROCESS;
        }

        return -1;
    }

    wxON_BLOCK_EXIT1(::CloseHandle, hProcess);

    // Default timeout for waiting for the process termination after killing
    // it. It should be long enough to allow the process to terminate even on a
    // busy system but short enough to avoid blocking the main thread for too
    // long.
    DWORD waitTimeout = 500; // ms

    bool ok = true;
    switch ( sig )
    {
        case wxSIGKILL:
            // kill the process forcefully returning -1 as error code
            if ( !::TerminateProcess(hProcess, (UINT)-1) )
            {
                wxLogSysError(_("Failed to kill process %d"), pid);

                if ( krc )
                {
                    // this is not supposed to happen if we could open the
                    // process
                    *krc = wxKILL_ERROR;
                }

                ok = false;
            }
            break;

        case wxSIGNONE:
            // Opening the process handle may succeed for a process even if it
            // doesn't run any more (typically because open handles to it still
            // exist elsewhere, possibly in this process itself if we're
            // killing a child process) so we still need check if it hasn't
            // terminated yet but, unlike when killing it, we don't need to
            // wait for any time at all.
            waitTimeout = 0;
            break;

        default:
            // any other signal means "terminate"
            {
                wxFindByPidParams params;
                params.pid = (DWORD)pid;

                // EnumWindows() has nice semantics: it returns 0 if it found
                // something or if an error occurred and non zero if it
                // enumerated all the window
                if ( !::EnumWindows(wxEnumFindByPidProc, (LPARAM)&params) )
                {
                    // did we find any window?
                    if ( params.hwnd )
                    {
                        // tell the app to close
                        //
                        // NB: this is the harshest way, the app won't have an
                        //     opportunity to save any files, for example, but
                        //     this is probably what we want here. If not we
                        //     can also use SendMesageTimeout(WM_CLOSE)
                        if ( !::PostMessage(params.hwnd, WM_QUIT, 0, 0) )
                        {
                            wxLogLastError(wxT("PostMessage(WM_QUIT)"));
                        }
                    }
                    else // it was an error then
                    {
                        wxLogLastError(wxT("EnumWindows"));

                        ok = false;
                    }
                }
                else // no windows for this PID
                {
                    if ( krc )
                        *krc = wxKILL_ERROR;

                    ok = false;
                }
            }
    }

    // the return code
    if ( ok )
    {
        // as we wait for a short time, we can use just WaitForSingleObject()
        // and not MsgWaitForMultipleObjects()
        switch ( ::WaitForSingleObject(hProcess, waitTimeout) )
        {
            case WAIT_OBJECT_0:
                // Process terminated: normally this indicates that we
                // successfully killed it but when testing for the process
                // existence, this means failure.
                if ( sig == wxSIGNONE )
                {
                    if ( krc )
                        *krc = wxKILL_NO_PROCESS;

                    ok = false;
                }
                break;

            default:
                wxFAIL_MSG( wxT("unexpected WaitForSingleObject() return") );
                // fall through

            case WAIT_FAILED:
                wxLogLastError(wxT("WaitForSingleObject"));
                // fall through

            case WAIT_TIMEOUT:
                // Process didn't terminate: normally this is a failure but not
                // when we're just testing for its existence.
                if ( sig != wxSIGNONE )
                {
                    if ( krc )
                        *krc = wxKILL_ERROR;

                    ok = false;
                }
                break;
        }
    }


    // the return code is the same as from Unix kill(): 0 if killed
    // successfully or -1 on error
    if ( !ok )
        return -1;

    if ( krc )
        *krc = wxKILL_OK;

    return 0;
}

typedef HANDLE (WINAPI *CreateToolhelp32Snapshot_t)(DWORD,DWORD);
typedef BOOL (WINAPI *Process32_t)(HANDLE,LPPROCESSENTRY32);

CreateToolhelp32Snapshot_t lpfCreateToolhelp32Snapshot;
Process32_t lpfProcess32First, lpfProcess32Next;

static void InitToolHelp32()
{
    static bool s_initToolHelpDone = false;

    if (s_initToolHelpDone)
        return;

    s_initToolHelpDone = true;

    lpfCreateToolhelp32Snapshot = NULL;
    lpfProcess32First = NULL;
    lpfProcess32Next = NULL;

#if wxUSE_DYNLIB_CLASS

    wxDynamicLibrary dllKernel(wxT("kernel32.dll"), wxDL_VERBATIM);

    // Get procedure addresses.
    // We are linking to these functions of Kernel32
    // explicitly, because otherwise a module using
    // this code would fail to load under Windows NT,
    // which does not have the Toolhelp32
    // functions in the Kernel 32.
    lpfCreateToolhelp32Snapshot =
        (CreateToolhelp32Snapshot_t)dllKernel.RawGetSymbol(wxT("CreateToolhelp32Snapshot"));

    lpfProcess32First =
        (Process32_t)dllKernel.RawGetSymbol(wxT("Process32First"));

    lpfProcess32Next =
        (Process32_t)dllKernel.RawGetSymbol(wxT("Process32Next"));

#endif // wxUSE_DYNLIB_CLASS
}

// By John Skiff
int wxKillAllChildren(long pid, wxSignal sig, wxKillError *krc)
{
    InitToolHelp32();

    if (krc)
        *krc = wxKILL_OK;

    // If not implemented for this platform (e.g. NT 4.0), silently ignore
    if (!lpfCreateToolhelp32Snapshot || !lpfProcess32First || !lpfProcess32Next)
        return 0;

    // Take a snapshot of all processes in the system.
    HANDLE hProcessSnap = lpfCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        if (krc)
            *krc = wxKILL_ERROR;
        return -1;
    }

    //Fill in the size of the structure before using it.
    PROCESSENTRY32 pe;
    wxZeroMemory(pe);
    pe.dwSize = sizeof(PROCESSENTRY32);

    // Walk the snapshot of the processes, and for each process,
    // kill it if its parent is pid.
    if (!lpfProcess32First(hProcessSnap, &pe)) {
        // Can't get first process.
        if (krc)
            *krc = wxKILL_ERROR;
        CloseHandle (hProcessSnap);
        return -1;
    }

    do {
        if (pe.th32ParentProcessID == (DWORD) pid) {
            if (wxKill(pe.th32ProcessID, sig, krc))
                return -1;
        }
    } while (lpfProcess32Next (hProcessSnap, &pe));


    return 0;
}

// Execute a program in an Interactive Shell
bool wxShell(const wxString& command)
{
    wxString cmd;

#ifdef __WXWINCE__
    cmd = command;
#else
    wxChar *shell = wxGetenv(wxT("COMSPEC"));
    if ( !shell )
        shell = (wxChar*) wxT("\\COMMAND.COM");

    if ( !command )
    {
        // just the shell
        cmd = shell;
    }
    else
    {
        // pass the command to execute to the command processor
        cmd.Printf(wxT("%s /c %s"), shell, command.c_str());
    }
#endif

    return wxExecute(cmd, wxEXEC_SYNC) == 0;
}

// Shutdown or reboot the PC
bool wxShutdown(int WXUNUSED_IN_WINCE(flags))
{
#ifdef __WXWINCE__
    // TODO-CE
    return false;
#elif defined(__WIN32__)
    bool bOK = true;

    if ( wxGetOsVersion(NULL, NULL) == wxOS_WINDOWS_NT ) // if is NT or 2K
    {
        // Get a token for this process.
        HANDLE hToken;
        bOK = ::OpenProcessToken(GetCurrentProcess(),
                                 TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                 &hToken) != 0;
        if ( bOK )
        {
            TOKEN_PRIVILEGES tkp;

            // Get the LUID for the shutdown privilege.
            bOK = ::LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
                                         &tkp.Privileges[0].Luid) != 0;

            if ( bOK )
            {
                tkp.PrivilegeCount = 1;  // one privilege to set
                tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                // Get the shutdown privilege for this process.
                ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                                        (PTOKEN_PRIVILEGES)NULL, 0);

                // Cannot test the return value of AdjustTokenPrivileges.
                bOK = ::GetLastError() == ERROR_SUCCESS;
            }

            ::CloseHandle(hToken);
        }
    }

    if ( bOK )
    {
        UINT wFlags = 0;
        if ( flags & wxSHUTDOWN_FORCE )
        {
            wFlags = EWX_FORCE;
            flags &= ~wxSHUTDOWN_FORCE;
        }

        switch ( flags )
        {
            case wxSHUTDOWN_POWEROFF:
                wFlags |= EWX_POWEROFF;
                break;

            case wxSHUTDOWN_REBOOT:
                wFlags |= EWX_REBOOT;
                break;

            case wxSHUTDOWN_LOGOFF:
                wFlags |= EWX_LOGOFF;
                break;

            default:
                wxFAIL_MSG( wxT("unknown wxShutdown() flag") );
                return false;
        }

        bOK = ::ExitWindowsEx(wFlags, 0) != 0;
    }

    return bOK;
#endif // WinCE/!WinCE
}

// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

// Get free memory in bytes, or -1 if cannot determine amount (e.g. on UNIX)
wxMemorySize wxGetFreeMemory()
{
#if defined(__WIN64__)
    MEMORYSTATUSEX memStatex;
    memStatex.dwLength = sizeof (memStatex);
    ::GlobalMemoryStatusEx (&memStatex);
    return (wxMemorySize)memStatex.ullAvailPhys;
#else /* if defined(__WIN32__) */
    MEMORYSTATUS memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUS);
    ::GlobalMemoryStatus(&memStatus);
    return (wxMemorySize)memStatus.dwAvailPhys;
#endif
}

unsigned long wxGetProcessId()
{
    return ::GetCurrentProcessId();
}

bool wxIsDebuggerRunning()
{
#if wxUSE_DYNLIB_CLASS
    // IsDebuggerPresent() is not available under Win95, so load it dynamically
    wxDynamicLibrary dll(wxT("kernel32.dll"), wxDL_VERBATIM);

    typedef BOOL (WINAPI *IsDebuggerPresent_t)();
    if ( !dll.HasSymbol(wxT("IsDebuggerPresent")) )
    {
        // no way to know, assume no
        return false;
    }

    return (*(IsDebuggerPresent_t)dll.GetSymbol(wxT("IsDebuggerPresent")))() != 0;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------
// working with MSW resources
// ----------------------------------------------------------------------------

bool
wxLoadUserResource(const void **outData,
                   size_t *outLen,
                   const wxString& resourceName,
                   const wxChar* resourceType,
                   WXHINSTANCE instance)
{
    wxCHECK_MSG( outData && outLen, false, "output pointers can't be NULL" );

    HRSRC hResource = ::FindResource(instance,
                                     resourceName.t_str(),
                                     resourceType);
    if ( !hResource )
        return false;

    HGLOBAL hData = ::LoadResource(instance, hResource);
    if ( !hData )
    {
        wxLogSysError(_("Failed to load resource \"%s\"."), resourceName);
        return false;
    }

    *outData = ::LockResource(hData);
    if ( !*outData )
    {
        wxLogSysError(_("Failed to lock resource \"%s\"."), resourceName);
        return false;
    }

    *outLen = ::SizeofResource(instance, hResource);

    // Notice that we do not need to call neither UnlockResource() (which is
    // obsolete in Win32) nor GlobalFree() (resources are freed on process
    // termination only)

    return true;
}

char *
wxLoadUserResource(const wxString& resourceName,
                   const wxChar* resourceType,
                   int* pLen,
                   WXHINSTANCE instance)
{
    const void *data;
    size_t len;
    if ( !wxLoadUserResource(&data, &len, resourceName, resourceType, instance) )
        return NULL;

    char *s = new char[len + 1];
    memcpy(s, data, len);
    s[len] = '\0'; // NUL-terminate in case the resource itself wasn't

    if (pLen)
      *pLen = len;

    return s;
}

// ----------------------------------------------------------------------------
// OS version
// ----------------------------------------------------------------------------

namespace
{

// Helper function wrapping Windows GetVersionEx() which is deprecated since
// Windows 8. For now, all we do in this wrapper is to avoid the deprecation
// warnings but this is not enough as the function now actually doesn't return
// the correct value any more and we need to use VerifyVersionInfo() to perform
// binary search to find the real Windows version.
OSVERSIONINFOEX wxGetWindowsVersionInfo()
{
    OSVERSIONINFOEX info;
    wxZeroMemory(info);

#ifdef __VISUALC__
    #pragma warning(push)
    #pragma warning(disable:4996) // 'xxx': was declared deprecated
#endif

    info.dwOSVersionInfoSize = sizeof(info);
    if ( !::GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&info)) )
    {
        // This really shouldn't ever happen.
        wxFAIL_MSG( "GetVersionEx() unexpectedly failed" );
    }

#ifdef __VISUALC__
    #pragma warning(pop)
#endif

    return info;
}

// check if we're running under a server or workstation Windows system: it
// returns true or false with obvious meaning as well as -1 if the system type
// couldn't be determined
//
// this function is currently private but we may want to expose it later if
// it's really useful

int wxIsWindowsServer()
{
#ifdef VER_NT_WORKSTATION
    switch ( wxGetWindowsVersionInfo().wProductType )
    {
        case VER_NT_WORKSTATION:
            return false;

        case VER_NT_SERVER:
        case VER_NT_DOMAIN_CONTROLLER:
            return true;
    }
#endif // VER_NT_WORKSTATION

    return -1;
}

} // anonymous namespace

wxString wxGetOsDescription()
{
    wxString str;

    const OSVERSIONINFOEX info = wxGetWindowsVersionInfo();
    switch ( info.dwPlatformId )
    {
#ifdef VER_PLATFORM_WIN32_CE
        case VER_PLATFORM_WIN32_CE:
            str.Printf(_("Windows CE (%d.%d)"),
                       info.dwMajorVersion,
                       info.dwMinorVersion);
            break;
#endif
        case VER_PLATFORM_WIN32s:
            str = _("Win32s on Windows 3.1");
            break;

        case VER_PLATFORM_WIN32_WINDOWS:
            switch (info.dwMinorVersion)
            {
                case 0:
                    if ( info.szCSDVersion[1] == 'B' ||
                         info.szCSDVersion[1] == 'C' )
                    {
                        str = _("Windows 95 OSR2");
                    }
                    else
                    {
                        str = _("Windows 95");
                    }
                    break;
                case 10:
                    if ( info.szCSDVersion[1] == 'B' ||
                         info.szCSDVersion[1] == 'C' )
                    {
                        str = _("Windows 98 SE");
                    }
                    else
                    {
                        str = _("Windows 98");
                    }
                    break;
                case 90:
                    str = _("Windows ME");
                    break;
                default:
                    str.Printf(_("Windows 9x (%d.%d)"),
                               info.dwMajorVersion,
                               info.dwMinorVersion);
                    break;
            }
            if ( !wxIsEmpty(info.szCSDVersion) )
            {
                str << wxT(" (") << info.szCSDVersion << wxT(')');
            }
            break;

        case VER_PLATFORM_WIN32_NT:
            switch ( info.dwMajorVersion )
            {
                case 5:
                    switch ( info.dwMinorVersion )
                    {
                        case 0:
                            str = _("Windows 2000");
                            break;

                        case 2:
                            // we can't distinguish between XP 64 and 2003
                            // as they both are 5.2, so examine the product
                            // type to resolve this ambiguity
                            if ( wxIsWindowsServer() == 1 )
                            {
                                str = _("Windows Server 2003");
                                break;
                            }
                            //else: must be XP, fall through

                        case 1:
                            str = _("Windows XP");
                            break;
                    }
                    break;

                case 6:
                    switch ( info.dwMinorVersion )
                    {
                        case 0:
                            str = wxIsWindowsServer() == 1
                                    ? _("Windows Server 2008")
                                    : _("Windows Vista");
                            break;

                        case 1:
                            str = wxIsWindowsServer() == 1
                                    ? _("Windows Server 2008 R2")
                                    : _("Windows 7");
                            break;

                        case 2:
                            str = wxIsWindowsServer() == 1
                                    ? _("Windows Server 2012")
                                    : _("Windows 8");
                            break;

                        case 3:
                            str = wxIsWindowsServer() == 1
                                    ? _("Windows Server 2012 R2")
                                    : _("Windows 8.1");
                            break;
                    }
                    break;

                case 10:
                    str = wxIsWindowsServer() == 1
                            ? _("Windows Server 2016")
                            : _("Windows 10");
                    break;
            }

            if ( str.empty() )
            {
                str.Printf(_("Windows NT %lu.%lu"),
                           info.dwMajorVersion,
                           info.dwMinorVersion);
            }

            str << wxT(" (")
                << wxString::Format(_("build %lu"), info.dwBuildNumber);
            if ( !wxIsEmpty(info.szCSDVersion) )
            {
                str << wxT(", ") << info.szCSDVersion;
            }
            str << wxT(')');

            if ( wxIsPlatform64Bit() )
                str << _(", 64-bit edition");
            break;
    }

    return str;
}

bool wxIsPlatform64Bit()
{
#if defined(__WIN64__)
    return true;  // 64-bit programs run only on Win64
#elif wxUSE_DYNLIB_CLASS // Win32
    // 32-bit programs run on both 32-bit and 64-bit Windows so check
    typedef BOOL (WINAPI *IsWow64Process_t)(HANDLE, BOOL *);

    wxDynamicLibrary dllKernel32(wxT("kernel32.dll"));
    IsWow64Process_t pfnIsWow64Process =
        (IsWow64Process_t)dllKernel32.RawGetSymbol(wxT("IsWow64Process"));

    BOOL wow64 = FALSE;
    if ( pfnIsWow64Process )
    {
        pfnIsWow64Process(::GetCurrentProcess(), &wow64);
    }
    //else: running under a system without Win64 support

    return wow64 != FALSE;
#else
    return false;
#endif // Win64/Win32
}

wxOperatingSystemId wxGetOsVersion(int *verMaj, int *verMin)
{
    static struct
    {
        bool initialized;

        wxOperatingSystemId os;

        int verMaj,
            verMin;
    } s_version;

    // query the OS info only once as it's not supposed to change
    if ( !s_version.initialized )
    {
        const OSVERSIONINFOEX info = wxGetWindowsVersionInfo();

        s_version.initialized = true;

#if defined(__WXWINCE__)
        s_version.os = wxOS_WINDOWS_CE;
#elif defined(__WXMICROWIN__)
        s_version.os = wxOS_WINDOWS_MICRO;
#else // "normal" desktop Windows system, use run-time detection
        switch ( info.dwPlatformId )
        {
            case VER_PLATFORM_WIN32_NT:
                s_version.os = wxOS_WINDOWS_NT;
                break;

            case VER_PLATFORM_WIN32_WINDOWS:
                s_version.os = wxOS_WINDOWS_9X;
                break;
        }
#endif // Windows versions

        s_version.verMaj = info.dwMajorVersion;
        s_version.verMin = info.dwMinorVersion;
    }

    if ( verMaj )
        *verMaj = s_version.verMaj;
    if ( verMin )
        *verMin = s_version.verMin;

    return s_version.os;
}

wxWinVersion wxGetWinVersion()
{
    int verMaj,
        verMin;
    switch ( wxGetOsVersion(&verMaj, &verMin) )
    {
        case wxOS_WINDOWS_9X:
            if ( verMaj == 4 )
            {
                switch ( verMin )
                {
                    case 0:
                        return wxWinVersion_95;

                    case 10:
                        return wxWinVersion_98;

                    case 90:
                        return wxWinVersion_ME;
                }
            }
            break;

        case wxOS_WINDOWS_NT:
            switch ( verMaj )
            {
                case 3:
                    return wxWinVersion_NT3;

                case 4:
                    return wxWinVersion_NT4;

                case 5:
                    switch ( verMin )
                    {
                        case 0:
                            return wxWinVersion_2000;

                        case 1:
                            return wxWinVersion_XP;

                        case 2:
                            return wxWinVersion_2003;
                    }
                    break;

                case 6:
                    switch ( verMin )
                    {
                        case 0:
                            return wxWinVersion_Vista;

                        case 1:
                            return wxWinVersion_7;

                        case 2:
                            return wxWinVersion_8;

                        case 3:
                            return wxWinVersion_8_1;

                    }
                    break;

                case 10:
                    return wxWinVersion_10;
            }
        default:
            // Do nothing just to silence GCC warning
            break;
    }

    return wxWinVersion_Unknown;
}

// ----------------------------------------------------------------------------
// sleep functions
// ----------------------------------------------------------------------------

void wxMilliSleep(unsigned long milliseconds)
{
    ::Sleep(milliseconds);
}

void wxMicroSleep(unsigned long microseconds)
{
    wxMilliSleep(microseconds/1000);
}

void wxSleep(int nSecs)
{
    wxMilliSleep(1000*nSecs);
}

// ----------------------------------------------------------------------------
// font encoding <-> Win32 codepage conversion functions
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_BASE long wxEncodingToCharset(wxFontEncoding encoding)
{
    switch ( encoding )
    {
        // although this function is supposed to return an exact match, do do
        // some mappings here for the most common case of "standard" encoding
        case wxFONTENCODING_SYSTEM:
            return DEFAULT_CHARSET;

        case wxFONTENCODING_ISO8859_1:
        case wxFONTENCODING_ISO8859_15:
        case wxFONTENCODING_CP1252:
            return ANSI_CHARSET;

#if !defined(__WXMICROWIN__)
        // The following four fonts are multi-byte charsets
        case wxFONTENCODING_CP932:
            return SHIFTJIS_CHARSET;

        case wxFONTENCODING_CP936:
            return GB2312_CHARSET;

#ifndef __WXWINCE__
        case wxFONTENCODING_CP949:
            return HANGUL_CHARSET;
#endif

        case wxFONTENCODING_CP950:
            return CHINESEBIG5_CHARSET;

        // The rest are single byte encodings
        case wxFONTENCODING_CP1250:
            return EASTEUROPE_CHARSET;

        case wxFONTENCODING_CP1251:
            return RUSSIAN_CHARSET;

        case wxFONTENCODING_CP1253:
            return GREEK_CHARSET;

        case wxFONTENCODING_CP1254:
            return TURKISH_CHARSET;

        case wxFONTENCODING_CP1255:
            return HEBREW_CHARSET;

        case wxFONTENCODING_CP1256:
            return ARABIC_CHARSET;

        case wxFONTENCODING_CP1257:
            return BALTIC_CHARSET;

        case wxFONTENCODING_CP874:
            return THAI_CHARSET;
#endif // !__WXMICROWIN__

        case wxFONTENCODING_CP437:
            return OEM_CHARSET;

        default:
            // no way to translate this encoding into a Windows charset
            return -1;
    }
}

// we have 2 versions of wxCharsetToCodepage(): the old one which directly
// looks up the vlaues in the registry and the new one which is more
// politically correct and has more chances to work on other Windows versions
// as well but the old version is still needed for !wxUSE_FONTMAP case
#if wxUSE_FONTMAP

#include "wx/fontmap.h"

extern WXDLLIMPEXP_BASE long wxEncodingToCodepage(wxFontEncoding encoding)
{
    // There don't seem to be symbolic names for
    // these under Windows so I just copied the
    // values from MSDN.

    unsigned int ret;

    switch (encoding)
    {
        case wxFONTENCODING_ISO8859_1:      ret = 28591; break;
        case wxFONTENCODING_ISO8859_2:      ret = 28592; break;
        case wxFONTENCODING_ISO8859_3:      ret = 28593; break;
        case wxFONTENCODING_ISO8859_4:      ret = 28594; break;
        case wxFONTENCODING_ISO8859_5:      ret = 28595; break;
        case wxFONTENCODING_ISO8859_6:      ret = 28596; break;
        case wxFONTENCODING_ISO8859_7:      ret = 28597; break;
        case wxFONTENCODING_ISO8859_8:      ret = 28598; break;
        case wxFONTENCODING_ISO8859_9:      ret = 28599; break;
        case wxFONTENCODING_ISO8859_10:     ret = 28600; break;
        case wxFONTENCODING_ISO8859_11:     ret = 874; break;
        // case wxFONTENCODING_ISO8859_12,      // doesn't exist currently, but put it
        case wxFONTENCODING_ISO8859_13:     ret = 28603; break;
        // case wxFONTENCODING_ISO8859_14:     ret = 28604; break; // no correspondence on Windows
        case wxFONTENCODING_ISO8859_15:     ret = 28605; break;

        case wxFONTENCODING_KOI8:           ret = 20866; break;
        case wxFONTENCODING_KOI8_U:         ret = 21866; break;

        case wxFONTENCODING_CP437:          ret = 437; break;
        case wxFONTENCODING_CP850:          ret = 850; break;
        case wxFONTENCODING_CP852:          ret = 852; break;
        case wxFONTENCODING_CP855:          ret = 855; break;
        case wxFONTENCODING_CP866:          ret = 866; break;
        case wxFONTENCODING_CP874:          ret = 874; break;
        case wxFONTENCODING_CP932:          ret = 932; break;
        case wxFONTENCODING_CP936:          ret = 936; break;
        case wxFONTENCODING_CP949:          ret = 949; break;
        case wxFONTENCODING_CP950:          ret = 950; break;
        case wxFONTENCODING_CP1250:         ret = 1250; break;
        case wxFONTENCODING_CP1251:         ret = 1251; break;
        case wxFONTENCODING_CP1252:         ret = 1252; break;
        case wxFONTENCODING_CP1253:         ret = 1253; break;
        case wxFONTENCODING_CP1254:         ret = 1254; break;
        case wxFONTENCODING_CP1255:         ret = 1255; break;
        case wxFONTENCODING_CP1256:         ret = 1256; break;
        case wxFONTENCODING_CP1257:         ret = 1257; break;

        case wxFONTENCODING_EUC_JP:         ret = 20932; break;

        case wxFONTENCODING_MACROMAN:       ret = 10000; break;
        case wxFONTENCODING_MACJAPANESE:    ret = 10001; break;
        case wxFONTENCODING_MACCHINESETRAD: ret = 10002; break;
        case wxFONTENCODING_MACKOREAN:      ret = 10003; break;
        case wxFONTENCODING_MACARABIC:      ret = 10004; break;
        case wxFONTENCODING_MACHEBREW:      ret = 10005; break;
        case wxFONTENCODING_MACGREEK:       ret = 10006; break;
        case wxFONTENCODING_MACCYRILLIC:    ret = 10007; break;
        case wxFONTENCODING_MACTHAI:        ret = 10021; break;
        case wxFONTENCODING_MACCHINESESIMP: ret = 10008; break;
        case wxFONTENCODING_MACCENTRALEUR:  ret = 10029; break;
        case wxFONTENCODING_MACCROATIAN:    ret = 10082; break;
        case wxFONTENCODING_MACICELANDIC:   ret = 10079; break;
        case wxFONTENCODING_MACROMANIAN:    ret = 10009; break;

        case wxFONTENCODING_ISO2022_JP:     ret = 50222; break;

        case wxFONTENCODING_UTF7:           ret = 65000; break;
        case wxFONTENCODING_UTF8:           ret = 65001; break;

        default:                            return -1;
    }

    if (::IsValidCodePage(ret) == 0)
        return -1;

    CPINFO info;
    if (::GetCPInfo(ret, &info) == 0)
        return -1;

    return (long) ret;
}

extern long wxCharsetToCodepage(const char *name)
{
    // first get the font encoding for this charset
    if ( !name )
        return -1;

    wxFontEncoding enc = wxFontMapperBase::Get()->CharsetToEncoding(name, false);
    if ( enc == wxFONTENCODING_SYSTEM )
        return -1;

    // the use the helper function
    return wxEncodingToCodepage(enc);
}

#else // !wxUSE_FONTMAP

#include "wx/msw/registry.h"

// this should work if Internet Exploiter is installed
extern long wxCharsetToCodepage(const char *name)
{
    if (!name)
        return GetACP();

    long CP = -1;

#if wxUSE_REGKEY
    wxString path(wxT("MIME\\Database\\Charset\\"));
    wxString cn(name);

    // follow the alias loop
    for ( ;; )
    {
        wxRegKey key(wxRegKey::HKCR, path + cn);

        if (!key.Exists())
            break;

        // two cases: either there's an AliasForCharset string,
        // or there are Codepage and InternetEncoding dwords.
        // The InternetEncoding gives us the actual encoding,
        // the Codepage just says which Windows character set to
        // use when displaying the data.
        if (key.HasValue(wxT("InternetEncoding")) &&
            key.QueryValue(wxT("InternetEncoding"), &CP))
            break;

        // no encoding, see if it's an alias
        if (!key.HasValue(wxT("AliasForCharset")) ||
            !key.QueryValue(wxT("AliasForCharset"), cn))
            break;
    }
#endif // wxUSE_REGKEY

    return CP;
}

#endif // wxUSE_FONTMAP/!wxUSE_FONTMAP

extern "C" WXDLLIMPEXP_BASE HWND
wxCreateHiddenWindow(LPCTSTR *pclassname, LPCTSTR classname, WNDPROC wndproc)
{
    wxCHECK_MSG( classname && pclassname && wndproc, NULL,
                    wxT("NULL parameter in wxCreateHiddenWindow") );

    // register the class fi we need to first
    if ( *pclassname == NULL )
    {
        WNDCLASS wndclass;
        wxZeroMemory(wndclass);

        wndclass.lpfnWndProc   = wndproc;
        wndclass.hInstance     = wxGetInstance();
        wndclass.lpszClassName = classname;

        if ( !::RegisterClass(&wndclass) )
        {
            wxLogLastError(wxT("RegisterClass() in wxCreateHiddenWindow"));

            return NULL;
        }

        *pclassname = classname;
    }

    // next create the window
    HWND hwnd = ::CreateWindow
                  (
                    *pclassname,
                    NULL,
                    0, 0, 0, 0,
                    0,
                    (HWND) NULL,
                    (HMENU)NULL,
                    wxGetInstance(),
                    (LPVOID) NULL
                  );

    if ( !hwnd )
    {
        wxLogLastError(wxT("CreateWindow() in wxCreateHiddenWindow"));
    }

    return hwnd;
}
