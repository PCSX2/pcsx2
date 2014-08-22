/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/glcanvas.h
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets under Macintosh
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GLCANVAS_H_
#define _WX_GLCANVAS_H_

#ifdef __WXOSX_IPHONE__
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#define wxUSE_OPENGL_EMULATION 1
#else
#include <OpenGL/gl.h>
#endif

#include "wx/vector.h"

// low level calls

WXDLLIMPEXP_GL WXGLContext WXGLCreateContext( WXGLPixelFormat pixelFormat, WXGLContext shareContext );
WXDLLIMPEXP_GL void WXGLDestroyContext( WXGLContext context );

WXDLLIMPEXP_GL WXGLContext WXGLGetCurrentContext();
WXDLLIMPEXP_GL bool WXGLSetCurrentContext(WXGLContext context);

WXDLLIMPEXP_GL WXGLPixelFormat WXGLChoosePixelFormat(const int *attribList);
WXDLLIMPEXP_GL void WXGLDestroyPixelFormat( WXGLPixelFormat pixelFormat );

class WXDLLIMPEXP_GL wxGLContext : public wxGLContextBase
{
public:
    wxGLContext(wxGLCanvas *win, const wxGLContext *other = NULL);
    virtual ~wxGLContext();

    virtual bool SetCurrent(const wxGLCanvas& win) const;

    // Mac-specific
    WXGLContext GetWXGLContext() const { return m_glContext; }

private:
    WXGLContext m_glContext;

    wxDECLARE_NO_COPY_CLASS(wxGLContext);
};

class WXDLLIMPEXP_GL wxGLCanvas : public wxGLCanvasBase
{
public:
    wxGLCanvas(wxWindow *parent,
               wxWindowID id = wxID_ANY,
               const int *attribList = NULL,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const wxPalette& palette = wxNullPalette);

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxGLCanvasName,
                const int *attribList = NULL,
                const wxPalette& palette = wxNullPalette);

    virtual ~wxGLCanvas();

    // implement wxGLCanvasBase methods
    virtual bool SwapBuffers();


    // Mac-specific functions
    // ----------------------

    // return true if multisample extension is supported
    static bool IsAGLMultiSampleAvailable();

    // return the pixel format used by this window
    WXGLPixelFormat GetWXGLPixelFormat() const { return m_glFormat; }

    // update the view port of the current context to match this window
    void SetViewport();


    // deprecated methods
    // ------------------

#if WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED(
    wxGLCanvas(wxWindow *parent,
               wxWindowID id = wxID_ANY,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const int *attribList = NULL,
               const wxPalette& palette = wxNullPalette)
    );

    wxDEPRECATED(
    wxGLCanvas(wxWindow *parent,
               const wxGLContext *shared,
               wxWindowID id = wxID_ANY,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const int *attribList = NULL,
               const wxPalette& palette = wxNullPalette)
    );

    wxDEPRECATED(
    wxGLCanvas(wxWindow *parent,
               const wxGLCanvas *shared,
               wxWindowID id = wxID_ANY,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const int *attribList = NULL,
               const wxPalette& palette = wxNullPalette)
    );
#endif // WXWIN_COMPATIBILITY_2_8

    // implementation-only from now on

#if wxOSX_USE_CARBON
    // Unlike some other platforms, this must get called if you override it,
    // i.e. don't forget "event.Skip()" in your EVT_SIZE handler
    void OnSize(wxSizeEvent& event);

    virtual void MacSuperChangedPosition();
    virtual void MacTopLevelWindowChangedPosition();
    virtual void MacVisibilityChanged();

    void MacUpdateView();

    GLint GetAglBufferName() const { return m_bufferName; }
#endif

protected:
    WXGLPixelFormat m_glFormat;

#if wxOSX_USE_CARBON
    bool m_macCanvasIsShown,
         m_needsUpdate;
    WXGLContext m_dummyContext;
    GLint m_bufferName;
#endif

    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(wxGLCanvas)
};

#endif // _WX_GLCANVAS_H_
