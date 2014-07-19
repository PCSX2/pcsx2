/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/private/overlay.h
// Purpose:     wxOverlayImpl declaration
// Author:      Vaclav Slavik
// Created:     2006-10-20
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_PRIVATE_OVERLAY_H_
#define _WX_DFB_PRIVATE_OVERLAY_H_

#include "wx/dfb/dfbptr.h"
#include "wx/gdicmn.h"

wxDFB_DECLARE_INTERFACE(IDirectFBSurface);

class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxDC;

class wxOverlayImpl
{
public:
    wxOverlayImpl();
    ~wxOverlayImpl();

    void Reset();
    bool IsOk();
    void Init(wxDC* dc, int x , int y , int width , int height);
    void BeginDrawing(wxDC* dc);
    void EndDrawing(wxDC* dc);
    void Clear(wxDC* dc);

    // wxDFB specific methods:
    bool IsEmpty() const { return m_isEmpty; }
    wxRect GetRect() const { return m_rect; }
    wxIDirectFBSurfacePtr GetDirectFBSurface() const { return m_surface; }

public:
    // window the overlay is associated with
    wxWindow *m_window;
    // rectangle covered by the overlay, in m_window's window coordinates
    wxRect m_rect;
    // surface of the overlay, same size as m_rect
    wxIDirectFBSurfacePtr m_surface;
    // this flag is set to true if nothing was drawn on the overlay (either
    // initially or Clear() was called)
    bool m_isEmpty;
};

#endif // _WX_DFB_PRIVATE_OVERLAY_H_
