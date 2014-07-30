/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/glcanvas.h
// Purpose:     wxGLCanvas class
// Author:      David Elliott
// Modified by:
// Created:     2004/09/29
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_GLCANVAS_H__
#define _WX_COCOA_GLCANVAS_H__

#include "wx/window.h"

// #include "wx/cocoa/NSOpenGLView.h"
// Include gl.h from the OpenGL framework
#include <OpenGL/gl.h>

class WXDLLIMPEXP_FWD_GL wxGLCanvas;
DECLARE_WXCOCOA_OBJC_CLASS(NSOpenGLContext);
DECLARE_WXCOCOA_OBJC_CLASS(NSOpenGLView);

// ========================================================================
// wxGLContext
// ========================================================================

class WXDLLIMPEXP_GL wxGLContext : public wxGLContextBase
{
public:
    wxGLContext(wxGLCanvas *win, const wxGLContext *other = NULL);

    virtual ~wxGLContext();

    virtual void SetCurrent(const wxGLCanvas& win) const;

    WX_NSOpenGLContext GetNSOpenGLContext() const
        { return m_cocoaNSOpenGLContext; }

private:
    WX_NSOpenGLContext m_cocoaNSOpenGLContext;
};

// ========================================================================
// wxGLCanvas
// ========================================================================

class WXDLLIMPEXP_GL wxGLCanvas : public wxGLCanvasBase
                             // , protected wxCocoaNSOpenGLView
{
    DECLARE_DYNAMIC_CLASS(wxGLCanvas)
//    WX_DECLARE_COCOA_OWNER(NSOpenGLView,NSView,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxGLCanvas(wxWindow *parent,
               wxWindowID id = wxID_ANY,
               const int *attribList = NULL,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const wxPalette& palette = wxNullPalette)
    {
        Create(parent, id, pos, size, style, name, attribList, palette);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxGLCanvasName,
                const int *attribList = NULL,
                const wxPalette& palette = wxNullPalette);

    virtual ~wxGLCanvas();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    // NSOpenGLView cannot be enabled/disabled
    virtual void CocoaSetEnabled(bool enable) { }
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual void SwapBuffers();


    NSOpenGLView *GetNSOpenGLView() const
        { return (NSOpenGLView *)m_cocoaNSView; }
};

#endif //ndef _WX_COCOA_GLCANVAS_H__
