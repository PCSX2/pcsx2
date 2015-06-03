/////////////////////////////////////////////////////////////////////////////
// Name:        wx/effects.h
// Purpose:     wxEffects class
//              Draws 3D effects.
// Author:      Julian Smart et al
// Modified by:
// Created:     25/4/2000
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_EFFECTS_H_
#define _WX_EFFECTS_H_

// this class is deprecated and will be removed in the next wx version
//
// please use wxRenderer::DrawBorder() instead of DrawSunkenEdge(); there is no
// replacement for TileBitmap() but it doesn't seem to be very useful anyhow
#if WXWIN_COMPATIBILITY_2_8

/*
 * wxEffects: various 3D effects
 */

#include "wx/object.h"
#include "wx/colour.h"
#include "wx/gdicmn.h"
#include "wx/dc.h"

class WXDLLIMPEXP_CORE wxEffectsImpl: public wxObject
{
public:
    // Assume system colours
    wxEffectsImpl() ;
    // Going from lightest to darkest
    wxEffectsImpl(const wxColour& highlightColour, const wxColour& lightShadow,
                  const wxColour& faceColour, const wxColour& mediumShadow,
                  const wxColour& darkShadow) ;

    // Accessors
    wxColour GetHighlightColour() const { return m_highlightColour; }
    wxColour GetLightShadow() const { return m_lightShadow; }
    wxColour GetFaceColour() const { return m_faceColour; }
    wxColour GetMediumShadow() const { return m_mediumShadow; }
    wxColour GetDarkShadow() const { return m_darkShadow; }

    void SetHighlightColour(const wxColour& c) { m_highlightColour = c; }
    void SetLightShadow(const wxColour& c) { m_lightShadow = c; }
    void SetFaceColour(const wxColour& c) { m_faceColour = c; }
    void SetMediumShadow(const wxColour& c) { m_mediumShadow = c; }
    void SetDarkShadow(const wxColour& c) { m_darkShadow = c; }

    void Set(const wxColour& highlightColour, const wxColour& lightShadow,
             const wxColour& faceColour, const wxColour& mediumShadow,
             const wxColour& darkShadow)
    {
        SetHighlightColour(highlightColour);
        SetLightShadow(lightShadow);
        SetFaceColour(faceColour);
        SetMediumShadow(mediumShadow);
        SetDarkShadow(darkShadow);
    }

    // Draw a sunken edge
    void DrawSunkenEdge(wxDC& dc, const wxRect& rect, int borderSize = 1);

    // Tile a bitmap
    bool TileBitmap(const wxRect& rect, wxDC& dc, const wxBitmap& bitmap);

protected:
    wxColour    m_highlightColour;  // Usually white
    wxColour    m_lightShadow;      // Usually light grey
    wxColour    m_faceColour;       // Usually grey
    wxColour    m_mediumShadow;     // Usually dark grey
    wxColour    m_darkShadow;       // Usually black

    wxDECLARE_CLASS(wxEffectsImpl);
};

// current versions of g++ don't generate deprecation warnings for classes
// declared deprecated, so define wxEffects as a typedef instead: this does
// generate warnings with both g++ and VC (which also has no troubles with
// directly deprecating the classes...)
//
// note that this g++ bug (16370) is supposed to be fixed in g++ 4.3.0
typedef wxEffectsImpl wxDEPRECATED(wxEffects);

#endif // WXWIN_COMPATIBILITY_2_8

#endif // _WX_EFFECTS_H_
