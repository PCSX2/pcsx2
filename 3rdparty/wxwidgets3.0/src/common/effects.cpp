/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/effects.cpp
// Purpose:     wxEffectsImpl implementation
// Author:      Julian Smart
// Modified by:
// Created:     25/4/2000
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/effects.h"

#ifndef WX_PRECOMP
    #include "wx/dcmemory.h"
    #include "wx/pen.h"
    #include "wx/settings.h"
    #include "wx/gdicmn.h"
#endif //WX_PRECOMP

#if WXWIN_COMPATIBILITY_2_8

/*
 * wxEffectsImpl: various 3D effects
 */

IMPLEMENT_CLASS(wxEffectsImpl, wxObject)

// Assume system colours
wxEffectsImpl::wxEffectsImpl()
{
    m_highlightColour = wxSystemSettings::GetColour(wxSYS_COLOUR_3DHILIGHT) ;
    m_lightShadow = wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT) ;
    m_faceColour = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE) ;
    m_mediumShadow = wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW) ;
    m_darkShadow = wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW) ;
}

// Going from lightest to darkest
wxEffectsImpl::wxEffectsImpl(const wxColour& highlightColour, const wxColour& lightShadow,
                const wxColour& faceColour, const wxColour& mediumShadow, const wxColour& darkShadow)
{
    m_highlightColour = highlightColour;
    m_lightShadow = lightShadow;
    m_faceColour = faceColour;
    m_mediumShadow = mediumShadow;
    m_darkShadow = darkShadow;
}

// Draw a sunken edge
void wxEffectsImpl::DrawSunkenEdge(wxDC& dc, const wxRect& rect, int WXUNUSED(borderSize))
{
    wxPen highlightPen(m_highlightColour, 1, wxPENSTYLE_SOLID);
    wxPen lightShadowPen(m_lightShadow, 1, wxPENSTYLE_SOLID);
    wxPen facePen(m_faceColour, 1, wxPENSTYLE_SOLID);
    wxPen mediumShadowPen(m_mediumShadow, 1, wxPENSTYLE_SOLID);
    wxPen darkShadowPen(m_darkShadow, 1, wxPENSTYLE_SOLID);

    //// LEFT AND TOP
    // Draw a medium shadow pen on left and top, followed by dark shadow line to
    // right and below of these lines

    dc.SetPen(mediumShadowPen);
    dc.DrawLine(rect.x, rect.y, rect.x+rect.width-1, rect.y); // Top
    dc.DrawLine(rect.x, rect.y, rect.x, rect.y+rect.height-1); // Left

    dc.SetPen(darkShadowPen);
    dc.DrawLine(rect.x+1, rect.y+1, rect.x+rect.width-2, rect.y+1); // Top
    dc.DrawLine(rect.x+1, rect.y+1, rect.x+1, rect.y+rect.height-1); // Left

    //// RIGHT AND BOTTOM

    dc.SetPen(highlightPen);
    dc.DrawLine(rect.x+rect.width-1, rect.y, rect.x+rect.width-1, rect.y+rect.height-1); // Right
    dc.DrawLine(rect.x, rect.y+rect.height-1, rect.x+rect.width, rect.y+rect.height-1); // Bottom

    dc.SetPen(lightShadowPen);
    dc.DrawLine(rect.x+rect.width-2, rect.y+1, rect.x+rect.width-2, rect.y+rect.height-2); // Right
    dc.DrawLine(rect.x+1, rect.y+rect.height-2, rect.x+rect.width-1, rect.y+rect.height-2); // Bottom

    dc.SetPen(wxNullPen);
}

bool wxEffectsImpl::TileBitmap(const wxRect& rect, wxDC& dc, const wxBitmap& bitmap)
{
    int w = bitmap.GetWidth();
    int h = bitmap.GetHeight();

    wxMemoryDC dcMem;

#if wxUSE_PALETTE
    static bool hiColour = (wxDisplayDepth() >= 16) ;
    if (bitmap.GetPalette() && !hiColour)
    {
        dc.SetPalette(* bitmap.GetPalette());
        dcMem.SetPalette(* bitmap.GetPalette());
    }
#endif // wxUSE_PALETTE

    dcMem.SelectObjectAsSource(bitmap);

    int i, j;
    for (i = rect.x; i < rect.x + rect.width; i += w)
    {
        for (j = rect.y; j < rect.y + rect.height; j+= h)
            dc.Blit(i, j, bitmap.GetWidth(), bitmap.GetHeight(), & dcMem, 0, 0);
    }
    dcMem.SelectObject(wxNullBitmap);

#if wxUSE_PALETTE
    if (bitmap.GetPalette() && !hiColour)
    {
        dc.SetPalette(wxNullPalette);
        dcMem.SetPalette(wxNullPalette);
    }
#endif // wxUSE_PALETTE

    return true;
}

#endif // WXWIN_COMPATIBILITY_2_8

