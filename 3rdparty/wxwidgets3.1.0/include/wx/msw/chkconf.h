/*
 * Name:        wx/msw/chkconf.h
 * Purpose:     Compiler-specific configuration checking
 * Author:      Julian Smart
 * Modified by:
 * Created:     01/02/97
 * Copyright:   (c) Julian Smart
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_MSW_CHKCONF_H_
#define _WX_MSW_CHKCONF_H_

/* ensure that MSW-specific settings are defined */
#ifndef wxUSE_ACTIVEX
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_ACTIVEX must be defined."
#    else
#        define wxUSE_ACTIVEX 0
#    endif
#endif /* !defined(wxUSE_ACTIVEX) */

#ifndef wxUSE_CRASHREPORT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CRASHREPORT must be defined."
#   else
#       define wxUSE_CRASHREPORT 0
#   endif
#endif /* !defined(wxUSE_CRASHREPORT) */

#ifndef wxUSE_DC_CACHEING
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DC_CACHEING must be defined"
#   else
#       define wxUSE_DC_CACHEING 1
#   endif
#endif /* wxUSE_DC_CACHEING */

#ifndef wxUSE_DIALUP_MANAGER
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_DIALUP_MANAGER must be defined."
#    else
#        define wxUSE_DIALUP_MANAGER 0
#    endif
#endif /* !defined(wxUSE_DIALUP_MANAGER) */

#ifndef wxUSE_MS_HTML_HELP
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_MS_HTML_HELP must be defined."
#    else
#        define wxUSE_MS_HTML_HELP 0
#    endif
#endif /* !defined(wxUSE_MS_HTML_HELP) */

#ifndef wxUSE_INICONF
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_INICONF must be defined."
#    else
#        define wxUSE_INICONF 0
#    endif
#endif /* !defined(wxUSE_INICONF) */

#ifndef wxUSE_OLE
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_OLE must be defined."
#    else
#        define wxUSE_OLE 0
#    endif
#endif /* !defined(wxUSE_OLE) */

#ifndef wxUSE_OLE_AUTOMATION
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_OLE_AUTOMATION must be defined."
#    else
#        define wxUSE_OLE_AUTOMATION 0
#    endif
#endif /* !defined(wxUSE_OLE_AUTOMATION) */

#ifndef wxUSE_TASKBARICON_BALLOONS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TASKBARICON_BALLOONS must be defined."
#   else
#       define wxUSE_TASKBARICON_BALLOONS 0
#   endif
#endif /* wxUSE_TASKBARICON_BALLOONS */

#ifndef wxUSE_TASKBARBUTTON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TASKBARBUTTON must be defined."
#   else
#       define wxUSE_TASKBARBUTTON 0
#   endif
#endif /* wxUSE_TASKBARBUTTON */

#ifndef wxUSE_UXTHEME
#    ifdef wxABORT_ON_CONFIG_ERROR
#        error "wxUSE_UXTHEME must be defined."
#    else
#        define wxUSE_UXTHEME 0
#    endif
#endif  /* wxUSE_UXTHEME */

/*
 * Unfortunately we can't use compiler TLS support if the library can be used
 * inside a dynamically loaded DLL under Windows XP, as this can result in hard
 * to diagnose crashes due to the bugs in Windows TLS support, see #13116.
 *
 * So we disable it unless we can be certain that the code will never run under
 * XP, as is the case if we're using a compiler which doesn't support XP such
 * as MSVC 11+, unless it's used with the special "_xp" toolset, in which case
 * _USING_V110_SDK71_ is defined.
 *
 * However defining wxUSE_COMPILER_TLS as 2 overrides this safety check, see
 * the comments in wx/setup.h.
 */
#if wxUSE_COMPILER_TLS == 1
    #if !wxCHECK_VISUALC_VERSION(11) || defined(_USING_V110_SDK71_)
        #undef wxUSE_COMPILER_TLS
        #define wxUSE_COMPILER_TLS 0
    #endif
#endif


/*
 * disable the settings which don't work for some compilers
 */

/*
 * All of the settings below require SEH support (__try/__catch) and can't work
 * without it.
 */
#if !defined(_MSC_VER) && \
    (!defined(__BORLANDC__) || __BORLANDC__ < 0x0550)
#    undef wxUSE_ON_FATAL_EXCEPTION
#    define wxUSE_ON_FATAL_EXCEPTION 0

#    undef wxUSE_CRASHREPORT
#    define wxUSE_CRASHREPORT 0

#    undef wxUSE_STACKWALKER
#    define wxUSE_STACKWALKER 0
#endif /* compiler doesn't support SEH */

#if defined(__GNUWIN32__)
    /* These don't work as expected for mingw32 and cygwin32 */
#   undef  wxUSE_MEMORY_TRACING
#   define wxUSE_MEMORY_TRACING            0

#   undef  wxUSE_GLOBAL_MEMORY_OPERATORS
#   define wxUSE_GLOBAL_MEMORY_OPERATORS   0

#   undef  wxUSE_DEBUG_NEW_ALWAYS
#   define wxUSE_DEBUG_NEW_ALWAYS          0

/* some Cygwin versions don't have wcslen */
#   if defined(__CYGWIN__) || defined(__CYGWIN32__)
#   if ! ((__GNUC__>2) ||((__GNUC__==2) && (__GNUC_MINOR__>=95)))
#       undef wxUSE_WCHAR_T
#       define wxUSE_WCHAR_T 0
#   endif
#endif

#endif /* __GNUWIN32__ */

#if !wxUSE_OWNER_DRAWN && !defined(__WXUNIVERSAL__)
#   undef wxUSE_CHECKLISTBOX
#   define wxUSE_CHECKLISTBOX 0
#endif

#if wxUSE_SPINCTRL
#   if !wxUSE_SPINBTN
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxSpinCtrl requires wxSpinButton on MSW"
#       else
#           undef wxUSE_SPINBTN
#           define wxUSE_SPINBTN 1
#       endif
#   endif
#endif

/*
   Win64-specific checks.
 */
#ifdef __WIN64__
#   if wxUSE_STACKWALKER
#       undef wxUSE_CRASHREPORT
#       define wxUSE_CRASHREPORT 0
#   endif
#endif /* __WIN64__ */


/*
   Compiler-specific checks.
 */

/* Borland */
#ifdef __BORLANDC__

#if __BORLANDC__ < 0x500
    /* BC++ 4.0 can't compile JPEG library */
#   undef wxUSE_LIBJPEG
#   define wxUSE_LIBJPEG 0
#endif

/* wxUSE_DEBUG_NEW_ALWAYS = 1 not compatible with BC++ in DLL mode */
#if defined(WXMAKINGDLL) || defined(WXUSINGDLL)
#   undef wxUSE_DEBUG_NEW_ALWAYS
#   define wxUSE_DEBUG_NEW_ALWAYS 0
#endif

#endif /* __BORLANDC__ */

/*
   un/redefine the options which we can't compile (after checking that they're
   defined
 */
#ifdef __WINE__
#   if wxUSE_ACTIVEX
#       undef wxUSE_ACTIVEX
#       define wxUSE_ACTIVEX 0
#   endif /* wxUSE_ACTIVEX */
#endif /* __WINE__ */

/*
    Currently wxUSE_GRAPHICS_CONTEXT is only enabled with MSVC by default, so
    only check for wxUSE_ACTIVITYINDICATOR dependency on it if it can be
    enabled, otherwise turn the latter off to allow the library to compile.
 */
#if !wxUSE_GRAPHICS_CONTEXT && !defined(_MSC_VER)
#   undef wxUSE_ACTIVITYINDICATOR
#   define wxUSE_ACTIVITYINDICATOR 0
#endif /* !wxUSE_ACTIVITYINDICATOR && !_MSC_VER */


/* check settings consistency for MSW-specific ones */
#if wxUSE_CRASHREPORT && !wxUSE_ON_FATAL_EXCEPTION
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CRASHREPORT requires wxUSE_ON_FATAL_EXCEPTION"
#   else
#       undef wxUSE_CRASHREPORT
#       define wxUSE_CRASHREPORT 0
#   endif
#endif /* wxUSE_CRASHREPORT */

#if !wxUSE_VARIANT
#   if wxUSE_ACTIVEX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxActiveXContainer requires wxVariant"
#       else
#           undef wxUSE_ACTIVEX
#           define wxUSE_ACTIVEX 0
#       endif
#   endif

#   if wxUSE_OLE_AUTOMATION
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxAutomationObject requires wxVariant"
#       else
#           undef wxUSE_OLE_AUTOMATION
#           define wxUSE_OLE_AUTOMATION 0
#       endif
#   endif
#endif /* !wxUSE_VARIANT */

#if !wxUSE_DYNAMIC_LOADER
#    if wxUSE_MS_HTML_HELP
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_MS_HTML_HELP requires wxUSE_DYNAMIC_LOADER."
#        else
#            undef wxUSE_MS_HTML_HELP
#            define wxUSE_MS_HTML_HELP 0
#        endif
#    endif
#    if wxUSE_DIALUP_MANAGER
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_DIALUP_MANAGER requires wxUSE_DYNAMIC_LOADER."
#        else
#            undef wxUSE_DIALUP_MANAGER
#            define wxUSE_DIALUP_MANAGER 0
#        endif
#    endif
#endif  /* !wxUSE_DYNAMIC_LOADER */

#if !wxUSE_DYNLIB_CLASS
#   if wxUSE_DC_TRANSFORM_MATRIX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_DC_TRANSFORM_MATRIX requires wxUSE_DYNLIB_CLASS"
#       else
#           undef wxUSE_DC_TRANSFORM_MATRIX
#           define wxUSE_DC_TRANSFORM_MATRIX 0
#       endif
#   endif
#   if wxUSE_UXTHEME
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_UXTHEME requires wxUSE_DYNLIB_CLASS"
#       else
#           undef wxUSE_UXTHEME
#           define wxUSE_UXTHEME 0
#       endif
#   endif
#   if wxUSE_MEDIACTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_MEDIACTRL requires wxUSE_DYNLIB_CLASS"
#       else
#           undef wxUSE_MEDIACTRL
#           define wxUSE_MEDIACTRL 0
#       endif
#   endif
#   if wxUSE_INKEDIT
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_INKEDIT requires wxUSE_DYNLIB_CLASS"
#       else
#           undef wxUSE_INKEDIT
#           define wxUSE_INKEDIT 0
#       endif
#   endif
#endif  /* !wxUSE_DYNLIB_CLASS */

#if !wxUSE_OLE
#   if wxUSE_ACTIVEX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxActiveXContainer requires wxUSE_OLE"
#       else
#           undef wxUSE_ACTIVEX
#           define wxUSE_ACTIVEX 0
#       endif
#   endif

#   if wxUSE_DATAOBJ
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_DATAOBJ requires wxUSE_OLE"
#       else
#           undef wxUSE_DATAOBJ
#           define wxUSE_DATAOBJ 0
#       endif
#   endif

#   if wxUSE_OLE_AUTOMATION
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxAutomationObject requires wxUSE_OLE"
#       else
#           undef wxUSE_OLE_AUTOMATION
#           define wxUSE_OLE_AUTOMATION 0
#       endif
#   endif
#endif /* !wxUSE_OLE */

#if !wxUSE_ACTIVEX
#   if wxUSE_MEDIACTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxMediaCtl requires wxActiveXContainer"
#       else
#           undef wxUSE_MEDIACTRL
#           define wxUSE_MEDIACTRL 0
#       endif
#   endif
#    if wxUSE_WEB
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxWebView requires wxActiveXContainer under MSW"
#       else
#           undef wxUSE_WEB
#           define wxUSE_WEB 0
#       endif
#   endif
#endif /* !wxUSE_ACTIVEX */

#if wxUSE_ACTIVITYINDICATOR && !wxUSE_GRAPHICS_CONTEXT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ACTIVITYINDICATOR requires wxGraphicsContext"
#   else
#       undef wxUSE_ACTIVITYINDICATOR
#       define wxUSE_ACTIVITYINDICATOR 0
#   endif
#endif /* wxUSE_ACTIVITYINDICATOR */

#if !wxUSE_THREADS
#   if wxUSE_FSWATCHER
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxFileSystemWatcher requires wxThread under MSW"
#       else
#           undef wxUSE_FSWATCHER
#           define wxUSE_FSWATCHER 0
#       endif
#   endif
#endif /* !wxUSE_THREADS */


#if !wxUSE_OLE_AUTOMATION
#    if wxUSE_WEB
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxWebView requires wxUSE_OLE_AUTOMATION under MSW"
#       else
#           undef wxUSE_WEB
#           define wxUSE_WEB 0
#       endif
#   endif
#endif /* !wxUSE_OLE_AUTOMATION */

#if defined(__WXUNIVERSAL__) && wxUSE_POSTSCRIPT_ARCHITECTURE_IN_MSW && !wxUSE_POSTSCRIPT
#   undef wxUSE_POSTSCRIPT
#   define wxUSE_POSTSCRIPT 1
#endif

#endif /* _WX_MSW_CHKCONF_H_ */
