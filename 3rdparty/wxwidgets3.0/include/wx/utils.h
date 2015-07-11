/////////////////////////////////////////////////////////////////////////////
// Name:        wx/utils.h
// Purpose:     Miscellaneous utilities
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UTILS_H_
#define _WX_UTILS_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/object.h"
#include "wx/list.h"
#include "wx/filefn.h"
#include "wx/hashmap.h"
#include "wx/versioninfo.h"
#include "wx/meta/implicitconversion.h"

#if wxUSE_GUI
    #include "wx/gdicmn.h"
    #include "wx/mousestate.h"
#endif

class WXDLLIMPEXP_FWD_BASE wxArrayString;
class WXDLLIMPEXP_FWD_BASE wxArrayInt;

// need this for wxGetDiskSpace() as we can't, unfortunately, forward declare
// wxLongLong
#include "wx/longlong.h"

// needed for wxOperatingSystemId, wxLinuxDistributionInfo
#include "wx/platinfo.h"

#ifdef __WATCOMC__
    #include <direct.h>
#elif defined(__X__)
    #include <dirent.h>
    #include <unistd.h>
#endif

#include <stdio.h>

// ----------------------------------------------------------------------------
// Forward declaration
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxProcess;
class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class wxWindowList;
class WXDLLIMPEXP_FWD_CORE wxEventLoop;

// ----------------------------------------------------------------------------
// Arithmetic functions
// ----------------------------------------------------------------------------

template<typename T1, typename T2>
inline typename wxImplicitConversionType<T1,T2>::value
wxMax(T1 a, T2 b)
{
    typedef typename wxImplicitConversionType<T1,T2>::value ResultType;

    // Cast both operands to the same type before comparing them to avoid
    // warnings about signed/unsigned comparisons from some compilers:
    return static_cast<ResultType>(a) > static_cast<ResultType>(b) ? a : b;
}

template<typename T1, typename T2>
inline typename wxImplicitConversionType<T1,T2>::value
wxMin(T1 a, T2 b)
{
    typedef typename wxImplicitConversionType<T1,T2>::value ResultType;

    return static_cast<ResultType>(a) < static_cast<ResultType>(b) ? a : b;
}

template<typename T1, typename T2, typename T3>
inline typename wxImplicitConversionType3<T1,T2,T3>::value
wxClip(T1 a, T2 b, T3 c)
{
    typedef typename wxImplicitConversionType3<T1,T2,T3>::value ResultType;

    if ( static_cast<ResultType>(a) < static_cast<ResultType>(b) )
        return b;

    if ( static_cast<ResultType>(a) > static_cast<ResultType>(c) )
        return c;

    return a;
}

// ----------------------------------------------------------------------------
// wxMemorySize
// ----------------------------------------------------------------------------

// wxGetFreeMemory can return huge amount of memory on 32-bit platforms as well
// so to always use long long for its result type on all platforms which
// support it
#if wxUSE_LONGLONG
    typedef wxLongLong wxMemorySize;
#else
    typedef long wxMemorySize;
#endif

// ----------------------------------------------------------------------------
// String functions (deprecated, use wxString)
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_8
// A shorter way of using strcmp
wxDEPRECATED_INLINE(inline bool wxStringEq(const char *s1, const char *s2),
    return wxCRT_StrcmpA(s1, s2) == 0; )

#if wxUSE_UNICODE
wxDEPRECATED_INLINE(inline bool wxStringEq(const wchar_t *s1, const wchar_t *s2),
    return wxCRT_StrcmpW(s1, s2) == 0; )
#endif // wxUSE_UNICODE

#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// Miscellaneous functions
// ----------------------------------------------------------------------------

// Sound the bell
WXDLLIMPEXP_CORE void wxBell();

#if wxUSE_MSGDLG
// Show wxWidgets information
WXDLLIMPEXP_CORE void wxInfoMessageBox(wxWindow* parent);
#endif // wxUSE_MSGDLG

WXDLLIMPEXP_CORE wxVersionInfo wxGetLibraryVersionInfo();

// Get OS description as a user-readable string
WXDLLIMPEXP_BASE wxString wxGetOsDescription();

// Get OS version
WXDLLIMPEXP_BASE wxOperatingSystemId wxGetOsVersion(int *majorVsn = NULL,
                                                    int *minorVsn = NULL);

// Get platform endianness
WXDLLIMPEXP_BASE bool wxIsPlatformLittleEndian();

// Get platform architecture
WXDLLIMPEXP_BASE bool wxIsPlatform64Bit();

#ifdef __LINUX__
// Get linux-distro informations
WXDLLIMPEXP_BASE wxLinuxDistributionInfo wxGetLinuxDistributionInfo();
#endif

// Return a string with the current date/time
WXDLLIMPEXP_BASE wxString wxNow();

// Return path where wxWidgets is installed (mostly useful in Unices)
WXDLLIMPEXP_BASE const wxChar *wxGetInstallPrefix();
// Return path to wxWin data (/usr/share/wx/%{version}) (Unices)
WXDLLIMPEXP_BASE wxString wxGetDataDir();

#if wxUSE_GUI

// Get the state of a key (true if pressed, false if not)
// This is generally most useful getting the state of
// the modifier or toggle keys.
WXDLLIMPEXP_CORE bool wxGetKeyState(wxKeyCode key);

// Don't synthesize KeyUp events holding down a key and producing
// KeyDown events with autorepeat. On by default and always on
// in wxMSW.
WXDLLIMPEXP_CORE bool wxSetDetectableAutoRepeat( bool flag );

// Returns the current state of the mouse position, buttons and modifers
WXDLLIMPEXP_CORE wxMouseState wxGetMouseState();

#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// wxPlatform
// ----------------------------------------------------------------------------

/*
 * Class to make it easier to specify platform-dependent values
 *
 * Examples:
 *  long val = wxPlatform::If(wxMac, 1).ElseIf(wxGTK, 2).ElseIf(stPDA, 5).Else(3);
 *  wxString strVal = wxPlatform::If(wxMac, wxT("Mac")).ElseIf(wxMSW, wxT("MSW")).Else(wxT("Other"));
 *
 * A custom platform symbol:
 *
 *  #define stPDA 100
 *  #ifdef __WXWINCE__
 *      wxPlatform::AddPlatform(stPDA);
 *  #endif
 *
 *  long windowStyle = wxCAPTION | (long) wxPlatform::IfNot(stPDA, wxRESIZE_BORDER);
 *
 */

class WXDLLIMPEXP_BASE wxPlatform
{
public:
    wxPlatform() { Init(); }
    wxPlatform(const wxPlatform& platform) { Copy(platform); }
    void operator = (const wxPlatform& platform) { if (&platform != this) Copy(platform); }
    void Copy(const wxPlatform& platform);

    // Specify an optional default value
    wxPlatform(int defValue) { Init(); m_longValue = (long)defValue; }
    wxPlatform(long defValue) { Init(); m_longValue = defValue; }
    wxPlatform(const wxString& defValue) { Init(); m_stringValue = defValue; }
    wxPlatform(double defValue) { Init(); m_doubleValue = defValue; }

    static wxPlatform If(int platform, long value);
    static wxPlatform IfNot(int platform, long value);
    wxPlatform& ElseIf(int platform, long value);
    wxPlatform& ElseIfNot(int platform, long value);
    wxPlatform& Else(long value);

    static wxPlatform If(int platform, int value) { return If(platform, (long)value); }
    static wxPlatform IfNot(int platform, int value) { return IfNot(platform, (long)value); }
    wxPlatform& ElseIf(int platform, int value) { return ElseIf(platform, (long) value); }
    wxPlatform& ElseIfNot(int platform, int value) { return ElseIfNot(platform, (long) value); }
    wxPlatform& Else(int value) { return Else((long) value); }

    static wxPlatform If(int platform, double value);
    static wxPlatform IfNot(int platform, double value);
    wxPlatform& ElseIf(int platform, double value);
    wxPlatform& ElseIfNot(int platform, double value);
    wxPlatform& Else(double value);

    static wxPlatform If(int platform, const wxString& value);
    static wxPlatform IfNot(int platform, const wxString& value);
    wxPlatform& ElseIf(int platform, const wxString& value);
    wxPlatform& ElseIfNot(int platform, const wxString& value);
    wxPlatform& Else(const wxString& value);

    long GetInteger() const { return m_longValue; }
    const wxString& GetString() const { return m_stringValue; }
    double GetDouble() const { return m_doubleValue; }

    operator int() const { return (int) GetInteger(); }
    operator long() const { return GetInteger(); }
    operator double() const { return GetDouble(); }
    operator const wxString&() const { return GetString(); }

    static void AddPlatform(int platform);
    static bool Is(int platform);
    static void ClearPlatforms();

private:

    void Init() { m_longValue = 0; m_doubleValue = 0.0; }

    long                m_longValue;
    double              m_doubleValue;
    wxString            m_stringValue;
    static wxArrayInt*  sm_customPlatforms;
};

/// Function for testing current platform
inline bool wxPlatformIs(int platform) { return wxPlatform::Is(platform); }

// ----------------------------------------------------------------------------
// Window ID management
// ----------------------------------------------------------------------------

// Ensure subsequent IDs don't clash with this one
WXDLLIMPEXP_BASE void wxRegisterId(int id);

// Return the current ID
WXDLLIMPEXP_BASE int wxGetCurrentId();

// Generate a unique ID
WXDLLIMPEXP_BASE int wxNewId();

// ----------------------------------------------------------------------------
// Various conversions
// ----------------------------------------------------------------------------

// Convert 2-digit hex number to decimal
WXDLLIMPEXP_BASE int wxHexToDec(const wxString& buf);

// Convert 2-digit hex number to decimal
inline int wxHexToDec(const char* buf)
{
    int firstDigit, secondDigit;

    if (buf[0] >= 'A')
        firstDigit = buf[0] - 'A' + 10;
    else
        firstDigit = buf[0] - '0';

    if (buf[1] >= 'A')
        secondDigit = buf[1] - 'A' + 10;
    else
        secondDigit = buf[1] - '0';

    return (firstDigit & 0xF) * 16 + (secondDigit & 0xF );
}


// Convert decimal integer to 2-character hex string
WXDLLIMPEXP_BASE void wxDecToHex(int dec, wxChar *buf);
WXDLLIMPEXP_BASE void wxDecToHex(int dec, char* ch1, char* ch2);
WXDLLIMPEXP_BASE wxString wxDecToHex(int dec);

// ----------------------------------------------------------------------------
// Process management
// ----------------------------------------------------------------------------

// NB: for backwards compatibility reasons the values of wxEXEC_[A]SYNC *must*
//     be 0 and 1, don't change!

enum
{
    // execute the process asynchronously
    wxEXEC_ASYNC    = 0,

    // execute it synchronously, i.e. wait until it finishes
    wxEXEC_SYNC     = 1,

    // under Windows, don't hide the child even if it's IO is redirected (this
    // is done by default)
    wxEXEC_SHOW_CONSOLE   = 2,

    // deprecated synonym for wxEXEC_SHOW_CONSOLE, use the new name as it's
    // more clear
    wxEXEC_NOHIDE = wxEXEC_SHOW_CONSOLE,

    // under Unix, if the process is the group leader then passing wxKILL_CHILDREN to wxKill
    // kills all children as well as pid
    // under Windows (NT family only), sets the CREATE_NEW_PROCESS_GROUP flag,
    // which allows to target Ctrl-Break signal to the spawned process.
    // applies to console processes only.
    wxEXEC_MAKE_GROUP_LEADER = 4,

    // by default synchronous execution disables all program windows to avoid
    // that the user interacts with the program while the child process is
    // running, you can use this flag to prevent this from happening
    wxEXEC_NODISABLE = 8,

    // by default, the event loop is run while waiting for synchronous execution
    // to complete and this flag can be used to simply block the main process
    // until the child process finishes
    wxEXEC_NOEVENTS = 16,

    // under Windows, hide the console of the child process if it has one, even
    // if its IO is not redirected
    wxEXEC_HIDE_CONSOLE = 32,

    // convenient synonym for flags given system()-like behaviour
    wxEXEC_BLOCK = wxEXEC_SYNC | wxEXEC_NOEVENTS
};

// Map storing environment variables.
typedef wxStringToStringHashMap wxEnvVariableHashMap;

// Used to pass additional parameters for child process to wxExecute(). Could
// be extended with other fields later.
struct wxExecuteEnv
{
    wxString cwd;               // If empty, CWD is not changed.
    wxEnvVariableHashMap env;   // If empty, environment is unchanged.
};

// Execute another program.
//
// If flags contain wxEXEC_SYNC, return -1 on failure and the exit code of the
// process if everything was ok. Otherwise (i.e. if wxEXEC_ASYNC), return 0 on
// failure and the PID of the launched process if ok.
WXDLLIMPEXP_BASE long wxExecute(const wxString& command,
                                int flags = wxEXEC_ASYNC,
                                wxProcess *process = NULL,
                                const wxExecuteEnv *env = NULL);
WXDLLIMPEXP_BASE long wxExecute(char **argv,
                                int flags = wxEXEC_ASYNC,
                                wxProcess *process = NULL,
                                const wxExecuteEnv *env = NULL);
#if wxUSE_UNICODE
WXDLLIMPEXP_BASE long wxExecute(wchar_t **argv,
                                int flags = wxEXEC_ASYNC,
                                wxProcess *process = NULL,
                                const wxExecuteEnv *env = NULL);
#endif // wxUSE_UNICODE

// execute the command capturing its output into an array line by line, this is
// always synchronous
WXDLLIMPEXP_BASE long wxExecute(const wxString& command,
                                wxArrayString& output,
                                int flags = 0,
                                const wxExecuteEnv *env = NULL);

// also capture stderr (also synchronous)
WXDLLIMPEXP_BASE long wxExecute(const wxString& command,
                                wxArrayString& output,
                                wxArrayString& error,
                                int flags = 0,
                                const wxExecuteEnv *env = NULL);

#if defined(__WINDOWS__) && wxUSE_IPC
// ask a DDE server to execute the DDE request with given parameters
WXDLLIMPEXP_BASE bool wxExecuteDDE(const wxString& ddeServer,
                                   const wxString& ddeTopic,
                                   const wxString& ddeCommand);
#endif // __WINDOWS__ && wxUSE_IPC

enum wxSignal
{
    wxSIGNONE = 0,  // verify if the process exists under Unix
    wxSIGHUP,
    wxSIGINT,
    wxSIGQUIT,
    wxSIGILL,
    wxSIGTRAP,
    wxSIGABRT,
    wxSIGIOT = wxSIGABRT,   // another name
    wxSIGEMT,
    wxSIGFPE,
    wxSIGKILL,
    wxSIGBUS,
    wxSIGSEGV,
    wxSIGSYS,
    wxSIGPIPE,
    wxSIGALRM,
    wxSIGTERM

    // further signals are different in meaning between different Unix systems
};

enum wxKillError
{
    wxKILL_OK,              // no error
    wxKILL_BAD_SIGNAL,      // no such signal
    wxKILL_ACCESS_DENIED,   // permission denied
    wxKILL_NO_PROCESS,      // no such process
    wxKILL_ERROR            // another, unspecified error
};

enum wxKillFlags
{
    wxKILL_NOCHILDREN = 0,  // don't kill children
    wxKILL_CHILDREN = 1     // kill children
};

enum wxShutdownFlags
{
    wxSHUTDOWN_FORCE    = 1,// can be combined with other flags (MSW-only)
    wxSHUTDOWN_POWEROFF = 2,// power off the computer
    wxSHUTDOWN_REBOOT   = 4,// shutdown and reboot
    wxSHUTDOWN_LOGOFF   = 8 // close session (currently MSW-only)
};

// Shutdown or reboot the PC
WXDLLIMPEXP_BASE bool wxShutdown(int flags = wxSHUTDOWN_POWEROFF);

// send the given signal to the process (only NONE and KILL are supported under
// Windows, all others mean TERM), return 0 if ok and -1 on error
//
// return detailed error in rc if not NULL
WXDLLIMPEXP_BASE int wxKill(long pid,
                       wxSignal sig = wxSIGTERM,
                       wxKillError *rc = NULL,
                       int flags = wxKILL_NOCHILDREN);

// Execute a command in an interactive shell window (always synchronously)
// If no command then just the shell
WXDLLIMPEXP_BASE bool wxShell(const wxString& command = wxEmptyString);

// As wxShell(), but must give a (non interactive) command and its output will
// be returned in output array
WXDLLIMPEXP_BASE bool wxShell(const wxString& command, wxArrayString& output);

// Sleep for nSecs seconds
WXDLLIMPEXP_BASE void wxSleep(int nSecs);

// Sleep for a given amount of milliseconds
WXDLLIMPEXP_BASE void wxMilliSleep(unsigned long milliseconds);

// Sleep for a given amount of microseconds
WXDLLIMPEXP_BASE void wxMicroSleep(unsigned long microseconds);

#if WXWIN_COMPATIBILITY_2_8
// Sleep for a given amount of milliseconds (old, bad name), use wxMilliSleep
wxDEPRECATED( WXDLLIMPEXP_BASE void wxUsleep(unsigned long milliseconds) );
#endif

// Get the process id of the current process
WXDLLIMPEXP_BASE unsigned long wxGetProcessId();

// Get free memory in bytes, or -1 if cannot determine amount (e.g. on UNIX)
WXDLLIMPEXP_BASE wxMemorySize wxGetFreeMemory();

#if wxUSE_ON_FATAL_EXCEPTION

// should wxApp::OnFatalException() be called?
WXDLLIMPEXP_BASE bool wxHandleFatalExceptions(bool doit = true);

#endif // wxUSE_ON_FATAL_EXCEPTION

// ----------------------------------------------------------------------------
// Environment variables
// ----------------------------------------------------------------------------

// returns true if variable exists (value may be NULL if you just want to check
// for this)
WXDLLIMPEXP_BASE bool wxGetEnv(const wxString& var, wxString *value);

// set the env var name to the given value, return true on success
WXDLLIMPEXP_BASE bool wxSetEnv(const wxString& var, const wxString& value);

// remove the env var from environment
WXDLLIMPEXP_BASE bool wxUnsetEnv(const wxString& var);

#if WXWIN_COMPATIBILITY_2_8
inline bool wxSetEnv(const wxString& var, const char *value)
    { return wxSetEnv(var, wxString(value)); }
inline bool wxSetEnv(const wxString& var, const wchar_t *value)
    { return wxSetEnv(var, wxString(value)); }
template<typename T>
inline bool wxSetEnv(const wxString& var, const wxScopedCharTypeBuffer<T>& value)
    { return wxSetEnv(var, wxString(value)); }
inline bool wxSetEnv(const wxString& var, const wxCStrData& value)
    { return wxSetEnv(var, wxString(value)); }

// this one is for passing NULL directly - don't use it, use wxUnsetEnv instead
wxDEPRECATED( inline bool wxSetEnv(const wxString& var, int value) );
inline bool wxSetEnv(const wxString& var, int value)
{
    wxASSERT_MSG( value == 0, "using non-NULL integer as string?" );

    wxUnusedVar(value); // fix unused parameter warning in release build

    return wxUnsetEnv(var);
}
#endif // WXWIN_COMPATIBILITY_2_8

// Retrieve the complete environment by filling specified map.
// Returns true on success or false if an error occurred.
WXDLLIMPEXP_BASE bool wxGetEnvMap(wxEnvVariableHashMap *map);

// ----------------------------------------------------------------------------
// Network and username functions.
// ----------------------------------------------------------------------------

// NB: "char *" functions are deprecated, use wxString ones!

// Get eMail address
WXDLLIMPEXP_BASE bool wxGetEmailAddress(wxChar *buf, int maxSize);
WXDLLIMPEXP_BASE wxString wxGetEmailAddress();

// Get hostname.
WXDLLIMPEXP_BASE bool wxGetHostName(wxChar *buf, int maxSize);
WXDLLIMPEXP_BASE wxString wxGetHostName();

// Get FQDN
WXDLLIMPEXP_BASE wxString wxGetFullHostName();
WXDLLIMPEXP_BASE bool wxGetFullHostName(wxChar *buf, int maxSize);

// Get user ID e.g. jacs (this is known as login name under Unix)
WXDLLIMPEXP_BASE bool wxGetUserId(wxChar *buf, int maxSize);
WXDLLIMPEXP_BASE wxString wxGetUserId();

// Get user name e.g. Julian Smart
WXDLLIMPEXP_BASE bool wxGetUserName(wxChar *buf, int maxSize);
WXDLLIMPEXP_BASE wxString wxGetUserName();

// Get current Home dir and copy to dest (returns pstr->c_str())
WXDLLIMPEXP_BASE wxString wxGetHomeDir();
WXDLLIMPEXP_BASE const wxChar* wxGetHomeDir(wxString *pstr);

// Get the user's (by default use the current user name) home dir,
// return empty string on error
WXDLLIMPEXP_BASE wxString wxGetUserHome(const wxString& user = wxEmptyString);


#if wxUSE_LONGLONG
    typedef wxLongLong wxDiskspaceSize_t;
#else
    typedef long wxDiskspaceSize_t;
#endif

// get number of total/free bytes on the disk where path belongs
WXDLLIMPEXP_BASE bool wxGetDiskSpace(const wxString& path,
                                     wxDiskspaceSize_t *pTotal = NULL,
                                     wxDiskspaceSize_t *pFree = NULL);



typedef int (*wxSortCallback)(const void* pItem1,
                              const void* pItem2,
                              const void* user_data);


WXDLLIMPEXP_BASE void wxQsort(void* pbase, size_t total_elems,
                              size_t size, wxSortCallback cmp,
                              const void* user_data);


#if wxUSE_GUI // GUI only things from now on

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

// flags for wxLaunchDefaultBrowser
enum
{
    wxBROWSER_NEW_WINDOW   = 0x01,
    wxBROWSER_NOBUSYCURSOR = 0x02
};

// Launch url in the user's default internet browser
WXDLLIMPEXP_CORE bool wxLaunchDefaultBrowser(const wxString& url, int flags = 0);

// Launch document in the user's default application
WXDLLIMPEXP_CORE bool wxLaunchDefaultApplication(const wxString& path, int flags = 0);

// ----------------------------------------------------------------------------
// Menu accelerators related things
// ----------------------------------------------------------------------------

// flags for wxStripMenuCodes
enum
{
    // strip '&' characters
    wxStrip_Mnemonics = 1,

    // strip everything after '\t'
    wxStrip_Accel = 2,

    // strip everything (this is the default)
    wxStrip_All = wxStrip_Mnemonics | wxStrip_Accel
};

// strip mnemonics and/or accelerators from the label
WXDLLIMPEXP_CORE wxString
wxStripMenuCodes(const wxString& str, int flags = wxStrip_All);

#if WXWIN_COMPATIBILITY_2_6
// obsolete and deprecated version, do not use, use the above overload instead
wxDEPRECATED(
    WXDLLIMPEXP_CORE wxChar* wxStripMenuCodes(const wxChar *in, wxChar *out = NULL)
);

#if wxUSE_ACCEL
class WXDLLIMPEXP_FWD_CORE wxAcceleratorEntry;

// use wxAcceleratorEntry::Create() or FromString() methods instead
wxDEPRECATED(
    WXDLLIMPEXP_CORE wxAcceleratorEntry *wxGetAccelFromString(const wxString& label)
);
#endif // wxUSE_ACCEL

#endif // WXWIN_COMPATIBILITY_2_6

// ----------------------------------------------------------------------------
// Window search
// ----------------------------------------------------------------------------

// Returns menu item id or wxNOT_FOUND if none.
WXDLLIMPEXP_CORE int wxFindMenuItemId(wxFrame *frame, const wxString& menuString, const wxString& itemString);

// Find the wxWindow at the given point. wxGenericFindWindowAtPoint
// is always present but may be less reliable than a native version.
WXDLLIMPEXP_CORE wxWindow* wxGenericFindWindowAtPoint(const wxPoint& pt);
WXDLLIMPEXP_CORE wxWindow* wxFindWindowAtPoint(const wxPoint& pt);

// NB: this function is obsolete, use wxWindow::FindWindowByLabel() instead
//
// Find the window/widget with the given title or label.
// Pass a parent to begin the search from, or NULL to look through
// all windows.
WXDLLIMPEXP_CORE wxWindow* wxFindWindowByLabel(const wxString& title, wxWindow *parent = NULL);

// NB: this function is obsolete, use wxWindow::FindWindowByName() instead
//
// Find window by name, and if that fails, by label.
WXDLLIMPEXP_CORE wxWindow* wxFindWindowByName(const wxString& name, wxWindow *parent = NULL);

// ----------------------------------------------------------------------------
// Message/event queue helpers
// ----------------------------------------------------------------------------

// Yield to other apps/messages and disable user input
WXDLLIMPEXP_CORE bool wxSafeYield(wxWindow *win = NULL, bool onlyIfNeeded = false);

// Enable or disable input to all top level windows
WXDLLIMPEXP_CORE void wxEnableTopLevelWindows(bool enable = true);

// Check whether this window wants to process messages, e.g. Stop button
// in long calculations.
WXDLLIMPEXP_CORE bool wxCheckForInterrupt(wxWindow *wnd);

// Consume all events until no more left
WXDLLIMPEXP_CORE void wxFlushEvents();

// a class which disables all windows (except, may be, the given one) in its
// ctor and enables them back in its dtor
class WXDLLIMPEXP_CORE wxWindowDisabler
{
public:
    // this ctor conditionally disables all windows: if the argument is false,
    // it doesn't do anything
    wxWindowDisabler(bool disable = true);

    // ctor disables all windows except winToSkip
    wxWindowDisabler(wxWindow *winToSkip);

    // dtor enables back all windows disabled by the ctor
    ~wxWindowDisabler();

private:
    // disable all windows except the given one (used by both ctors)
    void DoDisable(wxWindow *winToSkip = NULL);

#if defined(__WXOSX__) && wxOSX_USE_COCOA
    wxEventLoop* m_modalEventLoop;
#endif
    wxWindowList *m_winDisabled;
    bool m_disabled;

    wxDECLARE_NO_COPY_CLASS(wxWindowDisabler);
};

// ----------------------------------------------------------------------------
// Cursors
// ----------------------------------------------------------------------------

// Set the cursor to the busy cursor for all windows
WXDLLIMPEXP_CORE void wxBeginBusyCursor(const wxCursor *cursor = wxHOURGLASS_CURSOR);

// Restore cursor to normal
WXDLLIMPEXP_CORE void wxEndBusyCursor();

// true if we're between the above two calls
WXDLLIMPEXP_CORE bool wxIsBusy();

// Convenience class so we can just create a wxBusyCursor object on the stack
class WXDLLIMPEXP_CORE wxBusyCursor
{
public:
    wxBusyCursor(const wxCursor* cursor = wxHOURGLASS_CURSOR)
        { wxBeginBusyCursor(cursor); }
    ~wxBusyCursor()
        { wxEndBusyCursor(); }

    // Obsolete internal methods, do not use.
    static const wxCursor &GetStoredCursor();
    static const wxCursor GetBusyCursor();
};

void WXDLLIMPEXP_CORE wxGetMousePosition( int* x, int* y );

// ----------------------------------------------------------------------------
// X11 Display access
// ----------------------------------------------------------------------------

#if defined(__X__) || defined(__WXGTK__)

#ifdef __WXGTK__
    WXDLLIMPEXP_CORE void *wxGetDisplay();
#endif

#ifdef __X__
    WXDLLIMPEXP_CORE WXDisplay *wxGetDisplay();
    WXDLLIMPEXP_CORE bool wxSetDisplay(const wxString& display_name);
    WXDLLIMPEXP_CORE wxString wxGetDisplayName();
#endif // X or GTK+

// use this function instead of the functions above in implementation code
inline struct _XDisplay *wxGetX11Display()
{
    return (_XDisplay *)wxGetDisplay();
}

#endif // X11 || wxGTK

#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// wxYield(): these functions are obsolete, please use wxApp methods instead!
// ----------------------------------------------------------------------------

// avoid redeclaring this function here if it had been already declated by
// wx/app.h, this results in warnings from g++ with -Wredundant-decls
#ifndef wx_YIELD_DECLARED
#define wx_YIELD_DECLARED

// Yield to other apps/messages
WXDLLIMPEXP_CORE bool wxYield();

#endif // wx_YIELD_DECLARED

// Like wxYield, but fails silently if the yield is recursive.
WXDLLIMPEXP_CORE bool wxYieldIfNeeded();

// ----------------------------------------------------------------------------
// Windows resources access
// ----------------------------------------------------------------------------

// Windows only: get user-defined resource from the .res file.
#ifdef __WINDOWS__
    // default resource type for wxLoadUserResource()
    extern WXDLLIMPEXP_DATA_BASE(const wxChar*) wxUserResourceStr;

    // Return the pointer to the resource data. This pointer is read-only, use
    // the overload below if you need to modify the data.
    //
    // Notice that the resource type can be either a real string or an integer
    // produced by MAKEINTRESOURCE(). In particular, any standard resource type,
    // i.e any RT_XXX constant, could be passed here.
    //
    // Returns true on success, false on failure. Doesn't log an error message
    // if the resource is not found (because this could be expected) but does
    // log one if any other error occurs.
    WXDLLIMPEXP_BASE bool
    wxLoadUserResource(const void **outData,
                       size_t *outLen,
                       const wxString& resourceName,
                       const wxChar* resourceType = wxUserResourceStr,
                       WXHINSTANCE module = 0);

    // This function allocates a new buffer and makes a copy of the resource
    // data, remember to delete[] the buffer. And avoid using it entirely if
    // the overload above can be used.
    //
    // Returns NULL on failure.
    WXDLLIMPEXP_BASE char*
    wxLoadUserResource(const wxString& resourceName,
                       const wxChar* resourceType = wxUserResourceStr,
                       int* pLen = NULL,
                       WXHINSTANCE module = 0);
#endif // __WINDOWS__

#endif
    // _WX_UTILSH__
