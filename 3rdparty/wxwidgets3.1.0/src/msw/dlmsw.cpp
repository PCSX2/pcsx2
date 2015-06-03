/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dlmsw.cpp
// Purpose:     Win32-specific part of wxDynamicLibrary and related classes
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-01-10 (partly extracted from common/dynlib.cpp)
// Copyright:   (c) 1998-2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include  "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_DYNLIB_CLASS

#include "wx/msw/private.h"
#include "wx/msw/debughlp.h"
#include "wx/filename.h"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// wrap some functions from version.dll: load them dynamically and provide a
// clean interface
class wxVersionDLL
{
public:
    // load version.dll and bind to its functions
    wxVersionDLL();

    // return the file version as string, e.g. "x.y.z.w"
    wxString GetFileVersion(const wxString& filename) const;

private:
    typedef DWORD (APIENTRY *GetFileVersionInfoSize_t)(PTSTR, PDWORD);
    typedef BOOL (APIENTRY *GetFileVersionInfo_t)(PTSTR, DWORD, DWORD, PVOID);
    typedef BOOL (APIENTRY *VerQueryValue_t)(const PVOID, PTSTR, PVOID *, PUINT);

    #define DO_FOR_ALL_VER_FUNCS(what)                                        \
        what(GetFileVersionInfoSize);                                         \
        what(GetFileVersionInfo);                                             \
        what(VerQueryValue)

    #define DECLARE_VER_FUNCTION(func) func ## _t m_pfn ## func

    DO_FOR_ALL_VER_FUNCS(DECLARE_VER_FUNCTION);

    #undef DECLARE_VER_FUNCTION


    wxDynamicLibrary m_dll;


    wxDECLARE_NO_COPY_CLASS(wxVersionDLL);
};

// class used to create wxDynamicLibraryDetails objects
class WXDLLIMPEXP_BASE wxDynamicLibraryDetailsCreator
{
public:
    // type of parameters being passed to EnumModulesProc
    struct EnumModulesProcParams
    {
        wxDynamicLibraryDetailsArray *dlls;
        wxVersionDLL *verDLL;
    };

    // TODO: fix EnumerateLoadedModules() to use EnumerateLoadedModules64()
    #ifdef __WIN64__
        typedef DWORD64 DWORD_32_64;
    #else
        typedef DWORD DWORD_32_64;
    #endif

    static BOOL CALLBACK
    EnumModulesProc(PCSTR name, DWORD_32_64 base, ULONG size, void *data);
};

// ============================================================================
// wxVersionDLL implementation
// ============================================================================

// ----------------------------------------------------------------------------
// loading
// ----------------------------------------------------------------------------

wxVersionDLL::wxVersionDLL()
{
    // don't give errors if DLL can't be loaded or used, we're prepared to
    // handle it
    wxLogNull noLog;

    if ( m_dll.Load(wxT("version.dll"), wxDL_VERBATIM) )
    {
        // the functions we load have either 'A' or 'W' suffix depending on
        // whether we're in ANSI or Unicode build
        #ifdef UNICODE
            #define SUFFIX L"W"
        #else // ANSI
            #define SUFFIX "A"
        #endif // UNICODE/ANSI

        #define LOAD_VER_FUNCTION(name)                                       \
            m_pfn ## name = (name ## _t)m_dll.GetSymbol(wxT(#name SUFFIX));    \
        if ( !m_pfn ## name )                                                 \
        {                                                                     \
            m_dll.Unload();                                                   \
            return;                                                           \
        }

        DO_FOR_ALL_VER_FUNCS(LOAD_VER_FUNCTION);

        #undef LOAD_VER_FUNCTION
    }
}

// ----------------------------------------------------------------------------
// wxVersionDLL operations
// ----------------------------------------------------------------------------

wxString wxVersionDLL::GetFileVersion(const wxString& filename) const
{
    wxString ver;
    if ( m_dll.IsLoaded() )
    {
        wxChar *pc = const_cast<wxChar *>((const wxChar*) filename.t_str());

        DWORD dummy;
        DWORD sizeVerInfo = m_pfnGetFileVersionInfoSize(pc, &dummy);
        if ( sizeVerInfo )
        {
            wxCharBuffer buf(sizeVerInfo);
            if ( m_pfnGetFileVersionInfo(pc, 0, sizeVerInfo, buf.data()) )
            {
                void *pVer;
                UINT sizeInfo;
                if ( m_pfnVerQueryValue(buf.data(),
                                        const_cast<wxChar *>(wxT("\\")),
                                        &pVer,
                                        &sizeInfo) )
                {
                    VS_FIXEDFILEINFO *info = (VS_FIXEDFILEINFO *)pVer;
                    ver.Printf(wxT("%d.%d.%d.%d"),
                               HIWORD(info->dwFileVersionMS),
                               LOWORD(info->dwFileVersionMS),
                               HIWORD(info->dwFileVersionLS),
                               LOWORD(info->dwFileVersionLS));
                }
            }
        }
    }
    //else: we failed to load DLL, can't retrieve version info

    return ver;
}

// ============================================================================
// wxDynamicLibraryDetailsCreator implementation
// ============================================================================

/* static */
BOOL CALLBACK
wxDynamicLibraryDetailsCreator::EnumModulesProc(PCSTR name,
                                                DWORD_32_64 base,
                                                ULONG size,
                                                void *data)
{
    EnumModulesProcParams *params = (EnumModulesProcParams *)data;

    wxDynamicLibraryDetails *details = new wxDynamicLibraryDetails;

    // fill in simple properties
    details->m_name = name;
    details->m_address = wxUIntToPtr(base);
    details->m_length = size;

    // to get the version, we first need the full path
    const HMODULE hmod = wxDynamicLibrary::MSWGetModuleHandle
                         (
                            details->m_name,
                            details->m_address
                         );
    if ( hmod )
    {
        wxString fullname = wxGetFullModuleName(hmod);
        if ( !fullname.empty() )
        {
            details->m_path = fullname;
            details->m_version = params->verDLL->GetFileVersion(fullname);
        }
    }

    params->dlls->Add(details);

    // continue enumeration (returning FALSE would have stopped it)
    return TRUE;
}

// ============================================================================
// wxDynamicLibrary implementation
// ============================================================================

// ----------------------------------------------------------------------------
// misc functions
// ----------------------------------------------------------------------------

wxDllType wxDynamicLibrary::GetProgramHandle()
{
    return (wxDllType)::GetModuleHandle(NULL);
}

// ----------------------------------------------------------------------------
// loading/unloading DLLs
// ----------------------------------------------------------------------------

#ifndef MAX_PATH
    #define MAX_PATH 260        // from VC++ headers
#endif

/* static */
wxDllType
wxDynamicLibrary::RawLoad(const wxString& libname, int flags)
{
    if (flags & wxDL_GET_LOADED)
        return ::GetModuleHandle(libname.t_str());

    return ::LoadLibrary(libname.t_str());
}

/* static */
void wxDynamicLibrary::Unload(wxDllType handle)
{
    ::FreeLibrary(handle);
}

/* static */
void *wxDynamicLibrary::RawGetSymbol(wxDllType handle, const wxString& name)
{
    return (void *)::GetProcAddress(handle,
#ifdef __WXWINCE__
                                            name.t_str()
#else
                                            name.ToAscii()
#endif // __WXWINCE__
                                   );
}

// ----------------------------------------------------------------------------
// enumerating loaded DLLs
// ----------------------------------------------------------------------------

/* static */
wxDynamicLibraryDetailsArray wxDynamicLibrary::ListLoaded()
{
    wxDynamicLibraryDetailsArray dlls;

#if wxUSE_DBGHELP
    if ( wxDbgHelpDLL::Init() )
    {
        // prepare to use functions for version info extraction
        wxVersionDLL verDLL;

        wxDynamicLibraryDetailsCreator::EnumModulesProcParams params;
        params.dlls = &dlls;
        params.verDLL = &verDLL;

        // Note that the cast of EnumModulesProc is needed because the type of
        // PENUMLOADED_MODULES_CALLBACK changed: in old SDK versions its first
        // argument was non-const PSTR while now it's PCSTR. By explicitly
        // casting to whatever the currently used headers require we ensure
        // that the code compilers in any case.
        if ( !wxDbgHelpDLL::EnumerateLoadedModules
                            (
                                ::GetCurrentProcess(),
                                (PENUMLOADED_MODULES_CALLBACK)
                                wxDynamicLibraryDetailsCreator::EnumModulesProc,
                                &params
                            ) )
        {
            wxLogLastError(wxT("EnumerateLoadedModules"));
        }
    }
#endif // wxUSE_DBGHELP

    return dlls;
}

// ----------------------------------------------------------------------------
// Getting the module from an address inside it
// ----------------------------------------------------------------------------

namespace
{

// Tries to dynamically load GetModuleHandleEx() from kernel32.dll and call it
// to get the module handle from the given address. Returns NULL if it fails to
// either resolve the function (which can only happen on pre-Vista systems
// normally) or if the function itself failed.
HMODULE CallGetModuleHandleEx(const void* addr)
{
    typedef BOOL (WINAPI *GetModuleHandleEx_t)(DWORD, LPCTSTR, HMODULE *);
    static const GetModuleHandleEx_t INVALID_FUNC_PTR = (GetModuleHandleEx_t)-1;

    static GetModuleHandleEx_t s_pfnGetModuleHandleEx = INVALID_FUNC_PTR;
    if ( s_pfnGetModuleHandleEx == INVALID_FUNC_PTR )
    {
        wxDynamicLibrary dll(wxT("kernel32.dll"), wxDL_VERBATIM);

        wxDL_INIT_FUNC_AW(s_pfn, GetModuleHandleEx, dll);

        // dll object can be destroyed, kernel32.dll won't be unloaded anyhow
    }

    if ( !s_pfnGetModuleHandleEx )
        return NULL;

    // flags are GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
    //           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
    HMODULE hmod;
    if ( !s_pfnGetModuleHandleEx(6, (LPCTSTR)addr, &hmod) )
        return NULL;

    return hmod;
}

} // anonymous namespace

/* static */
void* wxDynamicLibrary::GetModuleFromAddress(const void* addr, wxString* path)
{
    HMODULE hmod = CallGetModuleHandleEx(addr);
    if ( !hmod )
    {
        wxLogLastError(wxT("GetModuleHandleEx"));
        return NULL;
    }

    if ( path )
    {
        TCHAR libname[MAX_PATH];
        if ( !::GetModuleFileName(hmod, libname, MAX_PATH) )
        {
            // GetModuleFileName could also return extended-length paths (paths
            // prepended with "//?/", maximum length is 32767 charachters) so,
            // in principle, MAX_PATH could be unsufficient and we should try
            // increasing the buffer size here.
            wxLogLastError(wxT("GetModuleFromAddress"));
            return NULL;
        }

        libname[MAX_PATH-1] = wxT('\0');

        *path = libname;
    }

    // In Windows HMODULE is actually the base address of the module so we
    // can just cast it to the address.
    return reinterpret_cast<void *>(hmod);
}

/* static */
WXHMODULE wxDynamicLibrary::MSWGetModuleHandle(const wxString& name, void *addr)
{
    // we want to use GetModuleHandleEx() instead of usual GetModuleHandle()
    // because the former works correctly for comctl32.dll while the latter
    // returns NULL when comctl32.dll version 6 is used under XP (note that
    // GetModuleHandleEx() is only available under XP and later, coincidence?)
    HMODULE hmod = CallGetModuleHandleEx(addr);

    return hmod ? hmod : ::GetModuleHandle(name.t_str());
}

#endif // wxUSE_DYNLIB_CLASS

