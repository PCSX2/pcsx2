///////////////////////////////////////////////////////////////////////////////
// Name:        wx/nativewin.h
// Purpose:     classes allowing to wrap a native window handle
// Author:      Vadim Zeitlin
// Created:     2008-03-05
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NATIVEWIN_H_
#define _WX_NATIVEWIN_H_

#include "wx/toplevel.h"

// this symbol can be tested in the user code to see if the current wx port has
// support for creating wxNativeContainerWindow from native windows
//
// be optimistic by default, we undefine it below if we don't have it finally
#define wxHAS_NATIVE_CONTAINER_WINDOW

// we define the following typedefs for each of the platform supporting native
// windows wrapping:
//
//  - wxNativeContainerWindowHandle is the toolkit-level handle of the native
//    window, i.e. HWND/GdkWindow*/NSWindow
//
//  - wxNativeContainerWindowId is the lowest level identifier of the native
//    window, i.e. HWND/GdkNativeWindow/NSWindow (so it's the same as above for
//    all platforms except GTK where we also can work with Window/XID)
//
// later we'll also have
//
//  - wxNativeWindowHandle for child windows (which will be wrapped by
//    wxNativeWindow<T> class), it is HWND/GtkWidget*/ControlRef
#if defined(__WXMSW__)
    #include "wx/msw/wrapwin.h"

    typedef HWND wxNativeContainerWindowId;
    typedef HWND wxNativeContainerWindowHandle;
#elif defined(__WXGTK__)
    // GdkNativeWindow is guint32 under GDK/X11 and gpointer under GDK/WIN32
    #ifdef __UNIX__
        typedef unsigned long wxNativeContainerWindowId;
    #else
        typedef void *wxNativeContainerWindowId;
    #endif
    typedef GdkWindow *wxNativeContainerWindowHandle;
#else
    // no support for using native windows under this platform yet
    #undef wxHAS_NATIVE_CONTAINER_WINDOW
#endif

#ifdef wxHAS_NATIVE_CONTAINER_WINDOW

// ----------------------------------------------------------------------------
// wxNativeContainerWindow: can be used for creating other wxWindows inside it
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNativeContainerWindow : public wxTopLevelWindow
{
public:
    // default ctor, call Create() later
    wxNativeContainerWindow() { }

    // create a window from an existing native window handle
    //
    // use GetHandle() to check if the creation was successful, it will return
    // 0 if the handle was invalid
    wxNativeContainerWindow(wxNativeContainerWindowHandle handle)
    {
        Create(handle);
    }

    // same as ctor above but with a return code
    bool Create(wxNativeContainerWindowHandle handle);

#if defined(__WXGTK__)
    // this is a convenient ctor for wxGTK applications which can also create
    // the objects of this class from the really native window handles and not
    // only the GdkWindow objects
    //
    // wxNativeContainerWindowId is Window (i.e. an XID, i.e. an int) under X11
    // (when GDK_WINDOWING_X11 is defined) or HWND under Win32
    wxNativeContainerWindow(wxNativeContainerWindowId winid) { Create(winid); }

    bool Create(wxNativeContainerWindowId winid);
#endif // wxGTK

    // unlike for the normal windows, dtor will not destroy the native window
    // as it normally doesn't belong to us
    virtual ~wxNativeContainerWindow();


    // provide (trivial) implementation of the base class pure virtuals
    virtual void SetTitle(const wxString& WXUNUSED(title))
    {
        wxFAIL_MSG( "not implemented for native windows" );
    }

    virtual wxString GetTitle() const
    {
        wxFAIL_MSG( "not implemented for native windows" );

        return wxString();
    }

    virtual void Maximize(bool WXUNUSED(maximize) = true)
    {
        wxFAIL_MSG( "not implemented for native windows" );
    }

    virtual bool IsMaximized() const
    {
        wxFAIL_MSG( "not implemented for native windows" );

        return false;
    }

    virtual void Iconize(bool WXUNUSED(iconize) = true)
    {
        wxFAIL_MSG( "not implemented for native windows" );
    }

    virtual bool IsIconized() const
    {
        // this is called by wxGTK implementation so don't assert
        return false;
    }

    virtual void Restore()
    {
        wxFAIL_MSG( "not implemented for native windows" );
    }

    virtual bool ShowFullScreen(bool WXUNUSED(show),
                                long WXUNUSED(style) = wxFULLSCREEN_ALL)
    {
        wxFAIL_MSG( "not implemented for native windows" );

        return false;
    }

    virtual bool IsFullScreen() const
    {
        wxFAIL_MSG( "not implemented for native windows" );

        return false;
    }

#ifdef __WXMSW__
    virtual bool IsShown() const;
#endif // __WXMSW__

    // this is an implementation detail: called when the native window is
    // destroyed by an outside agency; deletes the C++ object too but can in
    // principle be overridden to something else (knowing that the window
    // handle of this object and all of its children is invalid any more)
    virtual void OnNativeDestroyed();

protected:
#ifdef __WXMSW__
    virtual WXLRESULT
    MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif // __WXMSW__

private:
    wxDECLARE_NO_COPY_CLASS(wxNativeContainerWindow);
};

#endif // wxHAS_NATIVE_CONTAINER_WINDOW

#endif // _WX_NATIVEWIN_H_

