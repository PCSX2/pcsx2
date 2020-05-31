/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/glcanvas.cpp
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets under MS Windows
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

#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_GLCANVAS

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
#endif

#include "wx/msw/private.h"

#include "wx/glcanvas.h"

// from src/msw/window.cpp
LRESULT WXDLLEXPORT APIENTRY _EXPORT wxWndProc(HWND hWnd, UINT message,
                                   WPARAM wParam, LPARAM lParam);

#ifdef GL_EXT_vertex_array
    #define WXUNUSED_WITHOUT_GL_EXT_vertex_array(name) name
#else
    #define WXUNUSED_WITHOUT_GL_EXT_vertex_array(name) WXUNUSED(name)
#endif

// ----------------------------------------------------------------------------
// define possibly missing WGL constants and types
// ----------------------------------------------------------------------------

#ifndef WGL_ARB_pixel_format
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NUMBER_OVERLAYS_ARB           0x2008
#define WGL_NUMBER_UNDERLAYS_ARB          0x2009
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_STEREO_ARB                    0x2012
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_RED_BITS_ARB                  0x2015
#define WGL_GREEN_BITS_ARB                0x2017
#define WGL_BLUE_BITS_ARB                 0x2019
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_ACCUM_RED_BITS_ARB            0x201E
#define WGL_ACCUM_GREEN_BITS_ARB          0x201F
#define WGL_ACCUM_BLUE_BITS_ARB           0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB          0x2021
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_AUX_BUFFERS_ARB               0x2024
#define WGL_FULL_ACCELERATION_ARB         0x2027
#endif

#ifndef WGL_ARB_multisample
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#endif

#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context
#define WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB     0x2093
#define WGL_CONTEXT_FLAGS_ARB           0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB       0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002

#ifndef WGL_ARB_create_context_profile
#define WGL_ARB_create_context_profile
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

#ifndef WGL_ARB_create_context_robustness
#define WGL_ARB_create_context_robustness
#define WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB         0x00000004
#define WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB     0x8256
#define WGL_NO_RESET_NOTIFICATION_ARB                   0x8261
#define WGL_LOSE_CONTEXT_ON_RESET_ARB                   0x8252
#endif
#endif

#ifndef WGL_EXT_create_context_es2_profile
#define WGL_EXT_create_context_es2_profile
#define WGL_CONTEXT_ES2_PROFILE_BIT_EXT           0x00000004
#endif

#ifndef WGL_EXT_create_context_es_profile
#define WGL_EXT_create_context_es_profile
#define WGL_CONTEXT_ES_PROFILE_BIT_EXT            0x00000004
#endif

#ifndef WGL_ARB_framebuffer_sRGB
#define WGL_ARB_framebuffer_sRGB
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB                0x20A9
#endif

typedef HGLRC(WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)
    (HDC hDC, HGLRC hShareContext, const int *attribList);

// ----------------------------------------------------------------------------
// libraries
// ----------------------------------------------------------------------------

/*
  The following two compiler directives are specific to the Microsoft Visual
  C++ family of compilers

  Fundementally what they do is instruct the linker to use these two libraries
  for the resolution of symbols. In essence, this is the equivalent of adding
  these two libraries to either the Makefile or project file.

  This is NOT a recommended technique, and certainly is unlikely to be used
  anywhere else in wxWidgets given it is so specific to not only wxMSW, but
  also the VC compiler. However, in the case of opengl support, it's an
  applicable technique as opengl is optional in setup.h This code (wrapped by
  wxUSE_GLCANVAS), now allows opengl support to be added purely by modifying
  setup.h rather than by having to modify either the project or DSP fle.

  See MSDN for further information on the exact usage of these commands.
*/
#ifdef _MSC_VER
#  pragma comment( lib, "opengl32" )
#  pragma comment( lib, "glu32" )
#endif

// ----------------------------------------------------------------------------
// wxGLContext
// ----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxGLContext, wxObject)

// The window will always be created first so the array will be initialized
// and then the window will be assigned to the context.
// max 8 attributes plus terminator
// if first is 0, create legacy context
static int s_wglContextAttribs[9] = {0};

wxGLContext::wxGLContext(wxGLCanvas *win, const wxGLContext* other)
{
    if ( s_wglContextAttribs[0] == 0 ) // create legacy context
    {
        m_glContext = wglCreateContext(win->GetHDC());
        wxCHECK_RET( m_glContext, wxT("Couldn't create OpenGL context") );
    }
    else // create a context using attributes
    {
        // We need to create a temporary context to get the
        // wglCreateContextAttribsARB function
        HGLRC tempContext = wglCreateContext(win->GetHDC());
        wxCHECK_RET( tempContext, wxT("Couldn't create OpenGL context") );

        wglMakeCurrent(win->GetHDC(), tempContext);
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB
            = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
            wglGetProcAddress("wglCreateContextAttribsARB");
        wglMakeCurrent(win->GetHDC(), NULL);
        wglDeleteContext(tempContext);

        if ( !wglCreateContextAttribsARB )
        {
            wxLogError(_("Core OpenGL profile is not supported by the OpenGL driver."));
            return;
        }

        m_glContext = wglCreateContextAttribsARB(
            win->GetHDC(), 0, s_wglContextAttribs);
        wxCHECK_RET( m_glContext,
                     wxT("Couldn't create core profile OpenGL context") );
    }

    if ( other )
    {
        if ( !wglShareLists(other->m_glContext, m_glContext) )
        {
            wxLogLastError(wxT("wglShareLists"));
        }
    }
}

wxGLContext::~wxGLContext()
{
    // note that it's ok to delete the context even if it's the current one
    wglDeleteContext(m_glContext);
}

bool wxGLContext::SetCurrent(const wxGLCanvas& win) const
{
    if ( !wglMakeCurrent(win.GetHDC(), m_glContext) )
    {
        wxLogLastError(wxT("wglMakeCurrent"));
        return false;
    }
    return true;
}

// ============================================================================
// wxGLCanvas
// ============================================================================

IMPLEMENT_CLASS(wxGLCanvas, wxWindow)

BEGIN_EVENT_TABLE(wxGLCanvas, wxWindow)
#if wxUSE_PALETTE
    EVT_PALETTE_CHANGED(wxGLCanvas::OnPaletteChanged)
    EVT_QUERY_NEW_PALETTE(wxGLCanvas::OnQueryNewPalette)
#endif
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxGLCanvas construction
// ----------------------------------------------------------------------------

static int ChoosePixelFormatARB(HDC hdc, const int *attribList);

void wxGLCanvas::Init()
{
#if WXWIN_COMPATIBILITY_2_8
    m_glContext = NULL;
#endif
    m_hDC = NULL;
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       wxWindowID id,
                       const int *attribList,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const wxPalette& palette)
{
    Init();

    (void)Create(parent, id, pos, size, style, name, attribList, palette);
}

wxGLCanvas::~wxGLCanvas()
{
    ::ReleaseDC(GetHwnd(), m_hDC);
}

// Replaces wxWindow::Create functionality, since we need to use a different
// window class
bool wxGLCanvas::CreateWindow(wxWindow *parent,
                              wxWindowID id,
                              const wxPoint& pos,
                              const wxSize& size,
                              long style,
                              const wxString& name)
{
    wxCHECK_MSG( parent, false, wxT("can't create wxWindow without parent") );

    if ( !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    parent->AddChild(this);

    /*
       A general rule with OpenGL and Win32 is that any window that will have a
       HGLRC built for it must have two flags:  WS_CLIPCHILDREN & WS_CLIPSIBLINGS.
       You can find references about this within the knowledge base and most OpenGL
       books that contain the wgl function descriptions.
     */
    WXDWORD exStyle = 0;
    DWORD msflags = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    msflags |= MSWGetStyle(style, &exStyle);

    if ( !MSWCreate(wxApp::GetRegisteredClassName(wxT("wxGLCanvas"), -1, CS_OWNDC),
                    NULL, pos, size, msflags, exStyle) )
        return false;

    m_hDC = ::GetDC(GetHwnd());
    if ( !m_hDC )
        return false;

    return true;
}

bool wxGLCanvas::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name,
                        const int *attribList,
                        const wxPalette& palette)
{
    // Create the window first: we will either use it as is or use it to query
    // for multisampling support and recreate it later with another pixel format
    if ( !CreateWindow(parent, id, pos, size, style, name) )
        return false;

    // these will be used for the context creation attributes
    // if a core profile is requested
    bool useGLCoreProfile = false;

    // the minimum gl core version is 3.0
    int glVersionMajor = 3,
        glVersionMinor = 0;

    // Check for a core profile request
    if ( attribList )
    {
        for ( int i = 0; attribList[i]; )
        {
            switch ( attribList[i++] )
            {
                case WX_GL_CORE_PROFILE:
                    useGLCoreProfile = true;
                    break;

                case WX_GL_MAJOR_VERSION:
                    glVersionMajor = attribList[i++];
                    break;

                case WX_GL_MINOR_VERSION:
                    glVersionMinor = attribList[i++];
                    break;

                default:
                    // ignore all other flags for now
                    break;
            }
        }
    }

    if ( useGLCoreProfile )
    {
        s_wglContextAttribs[0] = WGL_CONTEXT_MAJOR_VERSION_ARB;
        s_wglContextAttribs[1] = glVersionMajor;
        s_wglContextAttribs[2] = WGL_CONTEXT_MINOR_VERSION_ARB;
        s_wglContextAttribs[3] = glVersionMinor;
        s_wglContextAttribs[4] = WGL_CONTEXT_FLAGS_ARB;
        s_wglContextAttribs[5] = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
        s_wglContextAttribs[6] = WGL_CONTEXT_PROFILE_MASK_ARB;
        s_wglContextAttribs[7] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
        s_wglContextAttribs[8] = 0; // terminate
    }
    else // create legacy/compatibility context
    {
        s_wglContextAttribs[0] = 0;
    }


    PIXELFORMATDESCRIPTOR pfd;
    const int setupVal = DoSetup(pfd, attribList);
    if ( setupVal == 0 ) // PixelFormat error
        return false;

    if ( setupVal == -1 ) // FSAA requested
    {
        // now that we have a valid OpenGL window, query it for FSAA support
        int pixelFormat;
        {
            wxGLContext ctx(this);
            ctx.SetCurrent(*this);
            pixelFormat = ::ChoosePixelFormatARB(m_hDC, attribList);
        }

        if ( pixelFormat > 0 )
        {
            // from http://msdn.microsoft.com/en-us/library/ms537559(VS.85).aspx:
            //
            //      Setting the pixel format of a window more than once can
            //      lead to significant complications for the Window Manager
            //      and for multithread applications, so it is not allowed. An
            //      application can only set the pixel format of a window one
            //      time. Once a window's pixel format is set, it cannot be
            //      changed.
            //
            // so we need to delete the old window and create the new one

            // destroy Window
            ::ReleaseDC(GetHwnd(), m_hDC);
            m_hDC = 0;

            parent->RemoveChild(this);
            const HWND hwnd = GetHwnd();
            DissociateHandle(); // will do SetHWND(0);
            ::DestroyWindow(hwnd);

            // now recreate with FSAA pixelFormat
            if ( !CreateWindow(parent, id, pos, size, style, name) )
                return false;

            if ( !::SetPixelFormat(m_hDC, pixelFormat, &pfd) )
            {
                wxLogLastError(wxT("SetPixelFormat"));
                return false;
            }
        }
    }

#if wxUSE_PALETTE
    if ( !SetupPalette(palette) )
        return false;
#else // !wxUSE_PALETTE
    wxUnusedVar(palette);
#endif // wxUSE_PALETTE/!wxUSE_PALETTE

    return true;
}

// ----------------------------------------------------------------------------
// operations
// ----------------------------------------------------------------------------

bool wxGLCanvas::SwapBuffers()
{
    if ( !::SwapBuffers(m_hDC) )
    {
        wxLogLastError(wxT("SwapBuffers"));
        return false;
    }

    return true;
}


// ----------------------------------------------------------------------------
// multi sample support
// ----------------------------------------------------------------------------

// this macro defines a variable of type "name_t" called "name" and initializes
// it with the pointer to WGL function "name" (which may be NULL)
#define wxDEFINE_WGL_FUNC(name) \
    name##_t name = (name##_t)wglGetProcAddress(#name)

/* static */
bool wxGLCanvasBase::IsExtensionSupported(const char *extension)
{
    static const char *s_extensionsList = (char *)wxUIntPtr(-1);
    if ( s_extensionsList == (char *)wxUIntPtr(-1) )
    {
        typedef const char * (WINAPI *wglGetExtensionsStringARB_t)(HDC hdc);

        wxDEFINE_WGL_FUNC(wglGetExtensionsStringARB);
        if ( wglGetExtensionsStringARB )
        {
            s_extensionsList = wglGetExtensionsStringARB(wglGetCurrentDC());
        }
        else
        {
            typedef const char * (WINAPI * wglGetExtensionsStringEXT_t)();

            wxDEFINE_WGL_FUNC(wglGetExtensionsStringEXT);
            if ( wglGetExtensionsStringEXT )
            {
                s_extensionsList = wglGetExtensionsStringEXT();
            }
            else
            {
                s_extensionsList = NULL;
            }
        }
    }

    return s_extensionsList && IsExtensionInList(s_extensionsList, extension);
}

// this is a wrapper around wglChoosePixelFormatARB(): returns the pixel format
// index matching the given attributes on success or 0 on failure
static int ChoosePixelFormatARB(HDC hdc, const int *attribList)
{
    if ( !wxGLCanvas::IsExtensionSupported("WGL_ARB_multisample") )
        return 0;

    typedef BOOL (WINAPI * wglChoosePixelFormatARB_t)
                 (HDC hdc,
                  const int *piAttribIList,
                  const FLOAT *pfAttribFList,
                  UINT nMaxFormats,
                  int *piFormats,
                  UINT *nNumFormats
                 );

    wxDEFINE_WGL_FUNC(wglChoosePixelFormatARB);
    if ( !wglChoosePixelFormatARB )
        return 0; // should not occur if extension is supported

    int iAttributes[128];
    int dst = 0; // index in iAttributes array

    #define ADD_ATTR(attr, value) \
        iAttributes[dst++] = attr; iAttributes[dst++] = value

    ADD_ATTR( WGL_DRAW_TO_WINDOW_ARB,    GL_TRUE );
    ADD_ATTR( WGL_SUPPORT_OPENGL_ARB,    GL_TRUE );
    ADD_ATTR( WGL_ACCELERATION_ARB,      WGL_FULL_ACCELERATION_ARB );

    if ( !attribList )
    {
        ADD_ATTR( WGL_COLOR_BITS_ARB,          24 );
        ADD_ATTR( WGL_ALPHA_BITS_ARB,           8 );
        ADD_ATTR( WGL_DEPTH_BITS_ARB,          16 );
        ADD_ATTR( WGL_STENCIL_BITS_ARB,         0 );
        ADD_ATTR( WGL_DOUBLE_BUFFER_ARB,  GL_TRUE );
        ADD_ATTR( WGL_SAMPLE_BUFFERS_ARB, GL_TRUE );
        ADD_ATTR( WGL_SAMPLES_ARB,              4 );
    }
    else // have custom attributes
    {
        #define ADD_ATTR_VALUE(attr) ADD_ATTR(attr, attribList[src++])

        int src = 0;
        while ( attribList[src] )
        {
            switch ( attribList[src++] )
            {
                case WX_GL_RGBA:
                    ADD_ATTR( WGL_COLOR_BITS_ARB, 24 );
                    ADD_ATTR( WGL_ALPHA_BITS_ARB,  8 );
                    break;

                case WX_GL_BUFFER_SIZE:
                    ADD_ATTR_VALUE( WGL_COLOR_BITS_ARB);
                    break;

                case WX_GL_LEVEL:
                    if ( attribList[src] > 0 )
                    {
                        ADD_ATTR( WGL_NUMBER_OVERLAYS_ARB, 1 );
                    }
                    else if ( attribList[src] <0 )
                    {
                        ADD_ATTR( WGL_NUMBER_UNDERLAYS_ARB, 1 );
                    }
                    //else: ignore it

                    src++; // skip the value in any case
                    break;

                case WX_GL_DOUBLEBUFFER:
                    ADD_ATTR( WGL_DOUBLE_BUFFER_ARB, GL_TRUE );
                    break;

                case WX_GL_STEREO:
                    ADD_ATTR( WGL_STEREO_ARB, GL_TRUE );
                    break;

                case WX_GL_AUX_BUFFERS:
                    ADD_ATTR_VALUE( WGL_AUX_BUFFERS_ARB );
                    break;

                case WX_GL_MIN_RED:
                    ADD_ATTR_VALUE( WGL_RED_BITS_ARB );
                    break;

                case WX_GL_MIN_GREEN:
                    ADD_ATTR_VALUE( WGL_GREEN_BITS_ARB );
                    break;

                case WX_GL_MIN_BLUE:
                    ADD_ATTR_VALUE( WGL_BLUE_BITS_ARB );
                    break;

                case WX_GL_MIN_ALPHA:
                    ADD_ATTR_VALUE( WGL_ALPHA_BITS_ARB );
                   break;

                case WX_GL_DEPTH_SIZE:
                    ADD_ATTR_VALUE( WGL_DEPTH_BITS_ARB );
                    break;

                case WX_GL_STENCIL_SIZE:
                    ADD_ATTR_VALUE( WGL_STENCIL_BITS_ARB );
                    break;

               case WX_GL_MIN_ACCUM_RED:
                    ADD_ATTR_VALUE( WGL_ACCUM_RED_BITS_ARB );
                    break;

                case WX_GL_MIN_ACCUM_GREEN:
                    ADD_ATTR_VALUE( WGL_ACCUM_GREEN_BITS_ARB );
                    break;

                case WX_GL_MIN_ACCUM_BLUE:
                    ADD_ATTR_VALUE( WGL_ACCUM_BLUE_BITS_ARB );
                    break;

                case WX_GL_MIN_ACCUM_ALPHA:
                    ADD_ATTR_VALUE( WGL_ACCUM_ALPHA_BITS_ARB );
                    break;

                case WX_GL_SAMPLE_BUFFERS:
                    ADD_ATTR_VALUE( WGL_SAMPLE_BUFFERS_ARB );
                    break;

                case WX_GL_SAMPLES:
                    ADD_ATTR_VALUE( WGL_SAMPLES_ARB );
                    break;
            }
        }

        #undef ADD_ATTR_VALUE
    }

    #undef ADD_ATTR

    iAttributes[dst++] = 0;

    int pf;
    UINT numFormats = 0;

    if ( !wglChoosePixelFormatARB(hdc, iAttributes, NULL, 1, &pf, &numFormats) )
    {
        wxLogLastError(wxT("wglChoosePixelFormatARB"));
        return 0;
    }

    // Although TRUE is returned if no matching formats are found (see
    // http://www.opengl.org/registry/specs/ARB/wgl_pixel_format.txt), pf is
    // not initialized in this case so we need to check for numFormats being
    // not 0 explicitly (however this is not an error so don't call
    // wxLogLastError() here).
    if ( !numFormats )
        pf = 0;

    return pf;
}

// ----------------------------------------------------------------------------
// pixel format stuff
// ----------------------------------------------------------------------------

// returns true if pfd was adjusted accordingly to attributes provided, false
// if there is an error with attributes or -1 if the attributes indicate
// features not supported by ChoosePixelFormat() at all (currently only multi
// sampling)
static int
AdjustPFDForAttributes(PIXELFORMATDESCRIPTOR& pfd, const int *attribList)
{
    if ( !attribList )
        return 1;

    // remove default attributes
    pfd.dwFlags &= ~PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_COLORINDEX;

    bool requestFSAA = false;
    for ( int arg = 0; attribList[arg]; )
    {
        switch ( attribList[arg++] )
        {
            case WX_GL_RGBA:
                pfd.iPixelType = PFD_TYPE_RGBA;
                break;

            case WX_GL_BUFFER_SIZE:
                pfd.cColorBits = attribList[arg++];
                break;

            case WX_GL_LEVEL:
                // this member looks like it may be obsolete
                if ( attribList[arg] > 0 )
                    pfd.iLayerType = PFD_OVERLAY_PLANE;
                else if ( attribList[arg] < 0 )
                    pfd.iLayerType = (BYTE)PFD_UNDERLAY_PLANE;
                else
                    pfd.iLayerType = PFD_MAIN_PLANE;
                arg++;
                break;

            case WX_GL_DOUBLEBUFFER:
                pfd.dwFlags |= PFD_DOUBLEBUFFER;
                break;

            case WX_GL_STEREO:
                pfd.dwFlags |= PFD_STEREO;
                break;

            case WX_GL_AUX_BUFFERS:
                pfd.cAuxBuffers = attribList[arg++];
                break;

            case WX_GL_MIN_RED:
                pfd.cColorBits += (pfd.cRedBits = attribList[arg++]);
                break;

            case WX_GL_MIN_GREEN:
                pfd.cColorBits += (pfd.cGreenBits = attribList[arg++]);
                break;

            case WX_GL_MIN_BLUE:
                pfd.cColorBits += (pfd.cBlueBits = attribList[arg++]);
                break;

            case WX_GL_MIN_ALPHA:
                // doesn't count in cColorBits
                pfd.cAlphaBits = attribList[arg++];
                break;

            case WX_GL_DEPTH_SIZE:
                pfd.cDepthBits = attribList[arg++];
                break;

            case WX_GL_STENCIL_SIZE:
                pfd.cStencilBits = attribList[arg++];
                break;

            case WX_GL_MIN_ACCUM_RED:
                pfd.cAccumBits += (pfd.cAccumRedBits = attribList[arg++]);
                break;

            case WX_GL_MIN_ACCUM_GREEN:
                pfd.cAccumBits += (pfd.cAccumGreenBits = attribList[arg++]);
                break;

            case WX_GL_MIN_ACCUM_BLUE:
                pfd.cAccumBits += (pfd.cAccumBlueBits = attribList[arg++]);
                break;

            case WX_GL_MIN_ACCUM_ALPHA:
                pfd.cAccumBits += (pfd.cAccumAlphaBits = attribList[arg++]);
                break;

            case WX_GL_SAMPLE_BUFFERS:
            case WX_GL_SAMPLES:
                // There is no support for multisample when using PIXELFORMATDESCRIPTOR
                requestFSAA = true; // Remember that multi sample is requested.
                arg++;              // will call ChoosePixelFormatARB() later
                break;
        }
    }

    return requestFSAA ? -1 : 1;
}

/* static */
int
wxGLCanvas::ChooseMatchingPixelFormat(HDC hdc,
                                      const int *attribList,
                                      PIXELFORMATDESCRIPTOR *ppfd)
{
    // default neutral pixel format
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),  // size
        1,                              // version
        PFD_SUPPORT_OPENGL |
        PFD_DRAW_TO_WINDOW |
        PFD_DOUBLEBUFFER,               // use double-buffering by default
        PFD_TYPE_RGBA,                  // default pixel type
        0,                              // preferred color depth (don't care)
        0, 0, 0, 0, 0, 0,               // color bits and shift bits (ignored)
        0, 0,                           // alpha bits and shift (ignored)
        0,                              // accumulation total bits
        0, 0, 0, 0,                     // accumulator RGBA bits (not used)
        16,                             // depth buffer
        0,                              // no stencil buffer
        0,                              // no auxiliary buffers
        PFD_MAIN_PLANE,                 // main layer
        0,                              // reserved
        0, 0, 0,                        // no layer, visible, damage masks
    };

    if ( !ppfd )
        ppfd = &pfd;
    else
        *ppfd = pfd;

    // adjust the PFD using the provided attributes and also check if we can
    // use PIXELFORMATDESCRIPTOR at all: if multisampling is requested, we
    // can't as it's not supported by ChoosePixelFormat()
    switch ( AdjustPFDForAttributes(*ppfd, attribList) )
    {
        case 1:
            return ::ChoosePixelFormat(hdc, ppfd);

        default:
            wxFAIL_MSG( "unexpected AdjustPFDForAttributes() return value" );
            // fall through

        case 0:
            // error in attributes
            return 0;

        case -1:
            // requestFSAA == true, will continue as normal
            // in order to query later for a FSAA pixelformat
            return -1;
    }
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const int *attribList)
{
    // We need a device context to test the pixel format, so get one
    // for the root window.
    return wxGLCanvas::ChooseMatchingPixelFormat(ScreenHDC(), attribList) > 0;
}

int wxGLCanvas::DoSetup(PIXELFORMATDESCRIPTOR &pfd, const int *attribList)
{
    int pixelFormat = ChooseMatchingPixelFormat(m_hDC, attribList, &pfd);

    const bool requestFSAA = pixelFormat == -1;
    if ( requestFSAA )
        pixelFormat = ::ChoosePixelFormat(m_hDC, &pfd);

    if ( !pixelFormat )
    {
        wxLogLastError(wxT("ChoosePixelFormat"));
        return 0;
    }

    if ( !::SetPixelFormat(m_hDC, pixelFormat, &pfd) )
    {
        wxLogLastError(wxT("SetPixelFormat"));
        return 0;
    }

    return requestFSAA ? -1 : 1;
}

// ----------------------------------------------------------------------------
// palette stuff
// ----------------------------------------------------------------------------

#if wxUSE_PALETTE

bool wxGLCanvas::SetupPalette(const wxPalette& palette)
{
    const int pixelFormat = ::GetPixelFormat(m_hDC);
    if ( !pixelFormat )
    {
        wxLogLastError(wxT("GetPixelFormat"));
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd;
    if ( !::DescribePixelFormat(m_hDC, pixelFormat, sizeof(pfd), &pfd) )
    {
        wxLogLastError(wxT("DescribePixelFormat"));
        return false;
    }

    if ( !(pfd.dwFlags & PFD_NEED_PALETTE) )
        return true;

    m_palette = palette;

    if ( !m_palette.IsOk() )
    {
        m_palette = CreateDefaultPalette();
        if ( !m_palette.IsOk() )
            return false;
    }

    if ( !::SelectPalette(m_hDC, GetHpaletteOf(m_palette), FALSE) )
    {
        wxLogLastError(wxT("SelectPalette"));
        return false;
    }

    if ( ::RealizePalette(m_hDC) == GDI_ERROR )
    {
        wxLogLastError(wxT("RealizePalette"));
        return false;
    }

    return true;
}

wxPalette wxGLCanvas::CreateDefaultPalette()
{
    PIXELFORMATDESCRIPTOR pfd;
    int paletteSize;
    int pixelFormat = GetPixelFormat(m_hDC);

    DescribePixelFormat(m_hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    paletteSize = 1 << pfd.cColorBits;

    LOGPALETTE* pPal =
     (LOGPALETTE*) malloc(sizeof(LOGPALETTE) + paletteSize * sizeof(PALETTEENTRY));
    pPal->palVersion = 0x300;
    pPal->palNumEntries = (WORD)paletteSize;

    /* build a simple RGB color palette */
    int redMask = (1 << pfd.cRedBits) - 1;
    int greenMask = (1 << pfd.cGreenBits) - 1;
    int blueMask = (1 << pfd.cBlueBits) - 1;

    for (int i=0; i<paletteSize; ++i)
    {
        pPal->palPalEntry[i].peRed =
            (BYTE)((((i >> pfd.cRedShift) & redMask) * 255) / redMask);
        pPal->palPalEntry[i].peGreen =
            (BYTE)((((i >> pfd.cGreenShift) & greenMask) * 255) / greenMask);
        pPal->palPalEntry[i].peBlue =
            (BYTE)((((i >> pfd.cBlueShift) & blueMask) * 255) / blueMask);
        pPal->palPalEntry[i].peFlags = 0;
    }

    HPALETTE hPalette = CreatePalette(pPal);
    free(pPal);

    wxPalette palette;
    palette.SetHPALETTE((WXHPALETTE) hPalette);

    return palette;
}

void wxGLCanvas::OnQueryNewPalette(wxQueryNewPaletteEvent& event)
{
  /* realize palette if this is the current window */
  if ( GetPalette()->IsOk() ) {
    ::UnrealizeObject((HPALETTE) GetPalette()->GetHPALETTE());
    ::SelectPalette(GetHDC(), (HPALETTE) GetPalette()->GetHPALETTE(), FALSE);
    ::RealizePalette(GetHDC());
    Refresh();
    event.SetPaletteRealized(true);
  }
  else
    event.SetPaletteRealized(false);
}

void wxGLCanvas::OnPaletteChanged(wxPaletteChangedEvent& event)
{
  /* realize palette if this is *not* the current window */
  if ( GetPalette() &&
       GetPalette()->IsOk() && (this != event.GetChangedWindow()) )
  {
    ::UnrealizeObject((HPALETTE) GetPalette()->GetHPALETTE());
    ::SelectPalette(GetHDC(), (HPALETTE) GetPalette()->GetHPALETTE(), FALSE);
    ::RealizePalette(GetHDC());
    Refresh();
  }
}

#endif // wxUSE_PALETTE

// ----------------------------------------------------------------------------
// deprecated wxGLCanvas methods using implicit wxGLContext
// ----------------------------------------------------------------------------

// deprecated constructors creating an implicit m_glContext
#if WXWIN_COMPATIBILITY_2_8

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const int *attribList,
                       const wxPalette& palette)
{
    Init();

    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLContext *shared,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const int *attribList,
                       const wxPalette& palette)
{
    Init();

    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this, shared);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent,
                       const wxGLCanvas *shared,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name,
                       const int *attribList,
                       const wxPalette& palette)
{
    Init();

    if ( Create(parent, id, pos, size, style, name, attribList, palette) )
        m_glContext = new wxGLContext(this, shared ? shared->m_glContext : NULL);
}

#endif // WXWIN_COMPATIBILITY_2_8


// ----------------------------------------------------------------------------
// wxGLApp
// ----------------------------------------------------------------------------

bool wxGLApp::InitGLVisual(const int *attribList)
{
    if ( !wxGLCanvas::ChooseMatchingPixelFormat(ScreenHDC(), attribList) )
    {
        wxLogError(_("Failed to initialize OpenGL"));
        return false;
    }

    return true;
}

#endif // wxUSE_GLCANVAS
