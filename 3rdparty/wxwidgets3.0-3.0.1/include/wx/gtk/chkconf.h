/*
 * Name:        wx/gtk/chkconf.h
 * Purpose:     wxGTK-specific settings consistency checks
 * Author:      Vadim Zeitlin
 * Created:     2007-07-19 (extracted from wx/chkconf.h)
 * Copyright:   (c) 2000-2007 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

#ifndef __WXUNIVERSAL__
#    if wxUSE_MDI_ARCHITECTURE && !wxUSE_MENUS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "MDI requires wxUSE_MENUS in wxGTK"
#        else
#            undef wxUSE_MENUS
#            define wxUSE_MENUS 1
#        endif
#    endif
#endif /* !__WXUNIVERSAL__ */

#if wxUSE_JOYSTICK
#    if !wxUSE_THREADS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxJoystick requires threads in wxGTK"
#        else
#            undef wxUSE_JOYSTICK
#            define wxUSE_JOYSTICK 0
#        endif
#    endif
#endif /* wxUSE_JOYSTICK */

#if wxUSE_POSTSCRIPT_ARCHITECTURE_IN_MSW && !wxUSE_POSTSCRIPT
#   undef  wxUSE_POSTSCRIPT
#   define wxUSE_POSTSCRIPT 1
#endif

#if wxUSE_OWNER_DRAWN
#   undef  wxUSE_OWNER_DRAWN
#   define wxUSE_OWNER_DRAWN 0
#endif

#if wxUSE_METAFILE
#   undef  wxUSE_METAFILE
#   define wxUSE_METAFILE 0
#endif

#if wxUSE_ENH_METAFILE
#   undef  wxUSE_ENH_METAFILE
#   define wxUSE_ENH_METAFILE 0
#endif

#ifndef __UNIX__

#   undef  wxUSE_WEBVIEW
#   define wxUSE_WEBVIEW 0
#   undef  wxUSE_WEBVIEW_WEBKIT
#   define wxUSE_WEBVIEW_WEBKIT 0

#   undef  wxUSE_MEDIACTRL
#   define wxUSE_MEDIACTRL 0

    /*
        We could use GDK_WINDOWING_X11 for those but this would require
        including gdk/gdk.h and we don't want to do it from here, so assume
        we're not using X11 if we're not under Unix.
     */

#   undef  wxUSE_UIACTIONSIMULATOR
#   define wxUSE_UIACTIONSIMULATOR 0

#   undef  wxUSE_GLCANVAS
#   define wxUSE_GLCANVAS 0

#endif /* __UNIX__ */

/*
    We always need Cairo with wxGTK, enable it if necessary (this can only
    happen under Windows).
 */
#ifdef __WINDOWS__

#if !wxUSE_CAIRO
#   undef  wxUSE_CAIRO
#   define wxUSE_CAIRO 1
#endif

#endif  /* __WINDOWS__ */

#ifdef __WXGTK3__
    #if !wxUSE_GRAPHICS_CONTEXT
        #ifdef wxABORT_ON_CONFIG_ERROR
            #error "GTK+ 3 support requires wxGraphicsContext"
        #else
            #undef wxUSE_GRAPHICS_CONTEXT
            #define wxUSE_GRAPHICS_CONTEXT 1
        #endif
    #endif
#endif
