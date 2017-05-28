///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/nonownedwnd.cpp
// Purpose:     wxNonOwnedWindow implementation for MSW.
// Author:      Vadim Zeitlin
// Created:     2011-10-09 (extracted from src/msw/toplevel.cpp)
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// This class can't be implemented and hence is not used under Win CE.
#ifndef __WXWINCE__

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/frame.h"       // Only for wxFRAME_SHAPED.
    #include "wx/region.h"
    #include "wx/msw/private.h"
#endif // WX_PRECOMP

#include "wx/nonownedwnd.h"

#if wxUSE_GRAPHICS_CONTEXT
    #include "wx/msw/wrapgdip.h"
    #include "wx/graphics.h"
#endif // wxUSE_GRAPHICS_CONTEXT

#include "wx/scopedptr.h"

// ============================================================================
// wxNonOwnedWindow implementation
// ============================================================================

bool wxNonOwnedWindow::DoClearShape()
{
    if (::SetWindowRgn(GetHwnd(), NULL, TRUE) == 0)
    {
        wxLogLastError(wxT("SetWindowRgn"));
        return false;
    }

    return true;
}

bool wxNonOwnedWindow::DoSetRegionShape(const wxRegion& region)
{
    // Windows takes ownership of the region, so
    // we'll have to make a copy of the region to give to it.
    DWORD noBytes = ::GetRegionData(GetHrgnOf(region), 0, NULL);
    RGNDATA *rgnData = (RGNDATA*) new char[noBytes];
    ::GetRegionData(GetHrgnOf(region), noBytes, rgnData);
    HRGN hrgn = ::ExtCreateRegion(NULL, noBytes, rgnData);
    delete[] (char*) rgnData;

    // SetWindowRgn expects the region to be in coordinates
    // relative to the window, not the client area.
    const wxPoint clientOrigin = GetClientAreaOrigin();
    ::OffsetRgn(hrgn, -clientOrigin.x, -clientOrigin.y);

    // Now call the shape API with the new region.
    if (::SetWindowRgn(GetHwnd(), hrgn, TRUE) == 0)
    {
        wxLogLastError(wxT("SetWindowRgn"));
        return false;
    }
    return true;
}

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/msw/wrapgdip.h"

// This class contains data used only when SetPath(wxGraphicsPath) is called.
//
// Notice that it derives from wxEvtHandler solely to allow Connect()-ing its
// OnPaint() method to the window, we could get rid of this inheritance once
// Bind() can be used in wx sources.
class wxNonOwnedWindowShapeImpl : public wxEvtHandler
{
public:
    wxNonOwnedWindowShapeImpl(wxNonOwnedWindow* win, const wxGraphicsPath& path) :
        m_win(win),
        m_path(path)
    {
        // Create the region corresponding to this path and set it as windows
        // shape.
        wxScopedPtr<wxGraphicsContext> context(wxGraphicsContext::Create(win));
        Region gr(static_cast<GraphicsPath*>(m_path.GetNativePath()));
        win->SetShape(
            wxRegion(
                gr.GetHRGN(static_cast<Graphics*>(context->GetNativeContext()))
            )
        );


        // Connect to the paint event to draw the border.
        //
        // TODO: Do this only optionally?
        m_win->Connect
               (
                wxEVT_PAINT,
                wxPaintEventHandler(wxNonOwnedWindowShapeImpl::OnPaint),
                NULL,
                this
               );
    }

    virtual ~wxNonOwnedWindowShapeImpl()
    {
        m_win->Disconnect
               (
                wxEVT_PAINT,
                wxPaintEventHandler(wxNonOwnedWindowShapeImpl::OnPaint),
                NULL,
                this
               );
    }

private:
    void OnPaint(wxPaintEvent& event)
    {
        event.Skip();

        wxPaintDC dc(m_win);
        wxScopedPtr<wxGraphicsContext> context(wxGraphicsContext::Create(dc));
        context->SetPen(wxPen(*wxLIGHT_GREY, 2));
        context->StrokePath(m_path);
    }

    wxNonOwnedWindow* const m_win;
    wxGraphicsPath m_path;

    wxDECLARE_NO_COPY_CLASS(wxNonOwnedWindowShapeImpl);
};

wxNonOwnedWindow::wxNonOwnedWindow()
{
    m_shapeImpl = NULL;
}

wxNonOwnedWindow::~wxNonOwnedWindow()
{
    delete m_shapeImpl;
}

bool wxNonOwnedWindow::DoSetPathShape(const wxGraphicsPath& path)
{
    delete m_shapeImpl;
    m_shapeImpl = new wxNonOwnedWindowShapeImpl(this, path);

    return true;
}

#else // !wxUSE_GRAPHICS_CONTEXT

// Trivial ctor and dtor as we don't have anything to do when wxGraphicsContext
// is not used but still define them here to avoid adding even more #if checks
// to the header, it it doesn't do any harm even though it's not needed.
wxNonOwnedWindow::wxNonOwnedWindow()
{
}

wxNonOwnedWindow::~wxNonOwnedWindow()
{
}

#endif // wxUSE_GRAPHICS_CONTEXT/!wxUSE_GRAPHICS_CONTEXT

#endif // !__WXWINCE__
