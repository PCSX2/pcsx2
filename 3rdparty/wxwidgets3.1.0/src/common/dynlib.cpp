/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dynlib.cpp
// Purpose:     Dynamic library management
// Author:      Guilhem Lavaux
// Modified by:
// Created:     20/07/98
// Copyright:   (c) 1998 Guilhem Lavaux
//                  2000-2005 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//FIXME:  This class isn't really common at all, it should be moved into
//        platform dependent files (already done for Windows and Unix)

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

#include "wx/dynlib.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/filefn.h"
#include "wx/filename.h"        // for SplitPath()
#include "wx/platinfo.h"

#include "wx/arrimpl.cpp"

WX_DEFINE_USER_EXPORTED_OBJARRAY(wxDynamicLibraryDetailsArray)

// ============================================================================
// implementation
// ============================================================================

// ---------------------------------------------------------------------------
// wxDynamicLibrary
// ---------------------------------------------------------------------------

// for MSW/Unix it is defined in platform-specific file
#if !(defined(__WINDOWS__) || defined(__UNIX__))

wxDllType wxDynamicLibrary::GetProgramHandle()
{
   wxFAIL_MSG( wxT("GetProgramHandle() is not implemented under this platform"));
   return 0;
}

#endif // __WINDOWS__ || __UNIX__


bool wxDynamicLibrary::Load(const wxString& libnameOrig, int flags)
{
    wxASSERT_MSG(m_handle == 0, wxT("Library already loaded."));

    // add the proper extension for the DLL ourselves unless told not to
    wxString libname = libnameOrig;
    if ( !(flags & wxDL_VERBATIM) )
    {
        // and also check that the libname doesn't already have it
        wxString ext;
        wxFileName::SplitPath(libname, NULL, NULL, &ext);
        if ( ext.empty() )
        {
            libname += GetDllExt(wxDL_MODULE);
        }
    }

    m_handle = RawLoad(libname, flags);

    if ( m_handle == 0 && !(flags & wxDL_QUIET) )
    {
#ifdef wxHAVE_DYNLIB_ERROR
        Error();
#else
        wxLogSysError(_("Failed to load shared library '%s'"), libname.c_str());
#endif
    }

    return IsLoaded();
}

void *wxDynamicLibrary::DoGetSymbol(const wxString &name, bool *success) const
{
    wxCHECK_MSG( IsLoaded(), NULL,
                 wxT("Can't load symbol from unloaded library") );

    void *symbol = RawGetSymbol(m_handle, name);

    if ( success )
        *success = symbol != NULL;

    return symbol;
}

void *wxDynamicLibrary::GetSymbol(const wxString& name, bool *success) const
{
    void *symbol = DoGetSymbol(name, success);
    if ( !symbol )
    {
#ifdef wxHAVE_DYNLIB_ERROR
        Error();
#else
        wxLogSysError(_("Couldn't find symbol '%s' in a dynamic library"),
                      name.c_str());
#endif
    }

    return symbol;
}

// ----------------------------------------------------------------------------
// informational methods
// ----------------------------------------------------------------------------

/*static*/
wxString wxDynamicLibrary::GetDllExt(wxDynamicLibraryCategory cat)
{
    wxUnusedVar(cat);
#if defined(__WINDOWS__)
    return ".dll";
#elif defined(__HPUX__)
    return ".sl";
#elif defined(__DARWIN__)
    switch ( cat )
    {
        case wxDL_LIBRARY:
            return ".dylib";
        case wxDL_MODULE:
            return ".bundle";
    }
    wxFAIL_MSG("unreachable");
    return wxString(); // silence gcc warning
#else
    return ".so";
#endif
}

/*static*/
wxString
wxDynamicLibrary::CanonicalizeName(const wxString& name,
                                   wxDynamicLibraryCategory cat)
{
    wxString nameCanonic;

    // under Unix the library names usually start with "lib" prefix, add it
#if defined(__UNIX__)
    switch ( cat )
    {
        case wxDL_LIBRARY:
            // Library names should start with "lib" under Unix.
            nameCanonic = "lib";
            break;
        case wxDL_MODULE:
            // Module names are arbitrary and should have no prefix added.
            break;
    }
#endif

    nameCanonic << name << GetDllExt(cat);

    return nameCanonic;
}

/*static*/
wxString wxDynamicLibrary::CanonicalizePluginName(const wxString& name,
                                                  wxPluginCategory cat)
{
    wxString suffix;
    if ( cat == wxDL_PLUGIN_GUI )
    {
        suffix = wxPlatformInfo::Get().GetPortIdShortName();
    }
#if wxUSE_UNICODE
    suffix << wxT('u');
#endif
#ifdef __WXDEBUG__
    suffix << wxT('d');
#endif

    if ( !suffix.empty() )
        suffix = wxString(wxT("_")) + suffix;

#define WXSTRINGIZE(x)  #x
#if defined(__UNIX__)
    #if (wxMINOR_VERSION % 2) == 0
        #define wxDLLVER(x,y,z) "-" WXSTRINGIZE(x) "." WXSTRINGIZE(y)
    #else
        #define wxDLLVER(x,y,z) "-" WXSTRINGIZE(x) "." WXSTRINGIZE(y) "." WXSTRINGIZE(z)
    #endif
#else
    #if (wxMINOR_VERSION % 2) == 0
        #define wxDLLVER(x,y,z) WXSTRINGIZE(x) WXSTRINGIZE(y)
    #else
        #define wxDLLVER(x,y,z) WXSTRINGIZE(x) WXSTRINGIZE(y) WXSTRINGIZE(z)
    #endif
#endif

    suffix << wxString::FromAscii(wxDLLVER(wxMAJOR_VERSION, wxMINOR_VERSION,
                                           wxRELEASE_NUMBER));
#undef wxDLLVER
#undef WXSTRINGIZE

#ifdef __WINDOWS__
    // Add compiler identification:
    #if defined(__GNUG__)
        suffix << wxT("_gcc");
    #elif defined(__VISUALC__)
        suffix << wxT("_vc");
    #elif defined(__BORLANDC__)
        suffix << wxT("_bcc");
    #endif
#endif

    return CanonicalizeName(name + suffix, wxDL_MODULE);
}

/*static*/
wxString wxDynamicLibrary::GetPluginsDirectory()
{
#ifdef __UNIX__
    wxString format = wxGetInstallPrefix();
    wxString dir;
    format << wxFILE_SEP_PATH
           << wxT("lib") << wxFILE_SEP_PATH
           << wxT("wx") << wxFILE_SEP_PATH
#if (wxMINOR_VERSION % 2) == 0
           << wxT("%i.%i");
    dir.Printf(format.c_str(), wxMAJOR_VERSION, wxMINOR_VERSION);
#else
           << wxT("%i.%i.%i");
    dir.Printf(format.c_str(),
               wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER);
#endif
    return dir;

#else // ! __UNIX__
    return wxEmptyString;
#endif
}


#endif // wxUSE_DYNLIB_CLASS
