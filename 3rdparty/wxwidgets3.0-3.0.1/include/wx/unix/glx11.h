///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/glx11.h
// Purpose:     class common for all X11-based wxGLCanvas implementations
// Author:      Vadim Zeitlin
// Created:     2007-04-15
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_GLX11_H_
#define _WX_UNIX_GLX11_H_

#include <GL/glx.h>

// ----------------------------------------------------------------------------
// wxGLContext
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_GL wxGLContext : public wxGLContextBase
{
public:
    wxGLContext(wxGLCanvas *win, const wxGLContext *other = NULL);
    virtual ~wxGLContext();

    virtual bool SetCurrent(const wxGLCanvas& win) const;

private:
    // attach context to the drawable or unset it (if NULL)
    static bool MakeCurrent(GLXDrawable drawable, GLXContext context);

    GLXContext m_glContext;

    DECLARE_CLASS(wxGLContext)
};

// ----------------------------------------------------------------------------
// wxGLCanvasX11
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_GL wxGLCanvasX11 : public wxGLCanvasBase
{
public:
    // initialization and dtor
    // -----------------------

    // default ctor doesn't do anything, InitVisual() must be called
    wxGLCanvasX11();

    // initializes the XVisualInfo corresponding to the given attributes
    bool InitVisual(const int *attribList);

    // frees XVisualInfo info
    virtual ~wxGLCanvasX11();


    // implement wxGLCanvasBase methods
    // --------------------------------

    virtual bool SwapBuffers();


    // X11-specific methods
    // --------------------

    // return GLX version: 13 means 1.3 &c
    static int GetGLXVersion();

    // return true if multisample extension is available
    static bool IsGLXMultiSampleAvailable();

    // get the X11 handle of this window
    virtual Window GetXWindow() const = 0;


    // override some wxWindow methods
    // ------------------------------

    // return true only if the window is realized: OpenGL context can't be
    // created until we are
    virtual bool IsShownOnScreen() const;


    // implementation only from now on
    // -------------------------------

    // get the GLXFBConfig/XVisualInfo we use
    GLXFBConfig *GetGLXFBConfig() const { return m_fbc; }
    XVisualInfo *GetXVisualInfo() const { return m_vi; }

    // initialize the global default GL visual, return false if matching visual
    // not found
    static bool InitDefaultVisualInfo(const int *attribList);

    // get the default GL X11 visual (may be NULL, shouldn't be freed by caller)
    static XVisualInfo *GetDefaultXVisualInfo() { return ms_glVisualInfo; }

    // free the global GL visual, called by wxGLApp
    static void FreeDefaultVisualInfo();

    // initializes XVisualInfo (in any case) and, if supported, GLXFBConfig
    //
    // returns false if XVisualInfo couldn't be initialized, otherwise caller
    // is responsible for freeing the pointers
    static bool InitXVisualInfo(const int *attribList,
                                GLXFBConfig **pFBC, XVisualInfo **pXVisual);

private:
    // fills in glattrs with attributes defined by wxattrs which must be
    // 0-terminated if it is non-NULL
    //
    // n is the max size of glattrs, false is returned if we overflow it, it
    // should be at least 16 to accommodate the default attributes
    static bool ConvertWXAttrsToGL(const int *wxattrs, int *glattrs, size_t n);


    // this is only used if it's supported i.e. if GL >= 1.3
    GLXFBConfig *m_fbc;

    // used for all GL versions, obtained from GLXFBConfig for GL >= 1.3
    XVisualInfo *m_vi;

    // the global/default versions of the above
    static GLXFBConfig *ms_glFBCInfo;
    static XVisualInfo *ms_glVisualInfo;
};

// ----------------------------------------------------------------------------
// wxGLApp
// ----------------------------------------------------------------------------

// this is used in wx/glcanvas.h, prevent it from defining a generic wxGLApp
#define wxGL_APP_DEFINED

class WXDLLIMPEXP_GL wxGLApp : public wxGLAppBase
{
public:
    wxGLApp() : wxGLAppBase() { }

    // implement wxGLAppBase method
    virtual bool InitGLVisual(const int *attribList)
    {
        return wxGLCanvasX11::InitDefaultVisualInfo(attribList);
    }

    // and implement this wxGTK::wxApp method too
    virtual void *GetXVisualInfo()
    {
        return wxGLCanvasX11::GetDefaultXVisualInfo();
    }

    // and override this wxApp method to clean up
    virtual int OnExit()
    {
        wxGLCanvasX11::FreeDefaultVisualInfo();

        return wxGLAppBase::OnExit();
    }

private:
    DECLARE_DYNAMIC_CLASS(wxGLApp)
};

#endif // _WX_UNIX_GLX11_H_

