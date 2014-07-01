/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/glcanvas.h
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets 2.0 for Motif.
//              Uses the GLX extension.
// Author:      Julian Smart and Wolfram Gloger
// Modified by:
// Created:     1995, 1999
// Copyright:   (c) Julian Smart, Wolfram Gloger
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GLCANVAS_H_
#define _WX_GLCANVAS_H_

#include "wx/unix/glx11.h"

class WXDLLIMPEXP_GL wxGLCanvas : public wxGLCanvasX11
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

    // implement wxGLCanvasX11 methods
    // --------------------------------

    virtual Window GetXWindow() const;

protected:
    virtual int GetColourIndex(const wxColour& col);

    DECLARE_CLASS(wxGLCanvas)
};

#endif // _WX_GLCANVAS_H_
