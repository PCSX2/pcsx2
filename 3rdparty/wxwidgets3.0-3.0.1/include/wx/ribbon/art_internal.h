///////////////////////////////////////////////////////////////////////////////
// Name:        wx/ribbon/art_internal.h
// Purpose:     Helper functions & classes used by ribbon art providers
// Author:      Peter Cawley
// Modified by:
// Created:     2009-08-04
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RIBBON_ART_INTERNAL_H_
#define _WX_RIBBON_ART_INTERNAL_H_

#include "wx/defs.h"

#if wxUSE_RIBBON

WXDLLIMPEXP_RIBBON wxColour wxRibbonInterpolateColour(
                                const wxColour& start_colour,
                                const wxColour& end_colour,
                                int position,
                                int start_position,
                                int end_position);

WXDLLIMPEXP_RIBBON bool wxRibbonCanLabelBreakAtPosition(
                                const wxString& label,
                                size_t pos);

WXDLLIMPEXP_RIBBON void wxRibbonDrawParallelGradientLines(
                                wxDC& dc,
                                int nlines,
                                const wxPoint* line_origins,
                                int stepx,
                                int stepy,
                                int numsteps,
                                int offset_x,
                                int offset_y,
                                const wxColour& start_colour,
                                const wxColour& end_colour);

WXDLLIMPEXP_RIBBON wxBitmap wxRibbonLoadPixmap(
                                const char* const* bits,
                                wxColour fore);

/*
   HSL colour class, using interface as discussed in wx-dev. Provided mainly
   for art providers to perform colour scheme calculations in the HSL colour
   space. If such a class makes it into base / core, then this class should be
   removed and users switched over to the one in base / core.

   0.0 <= Hue < 360.0
   0.0 <= Saturation <= 1.0
   0.0 <= Luminance <= 1.0
*/
class WXDLLIMPEXP_RIBBON wxRibbonHSLColour
{
public:
   wxRibbonHSLColour()
       : hue(0.0), saturation(0.0), luminance(0.0) {}
   wxRibbonHSLColour(float H, float S, float L)
       : hue(H), saturation(S), luminance(L) { }
   wxRibbonHSLColour(const wxColour& C);

   wxColour    ToRGB() const;

   wxRibbonHSLColour& MakeDarker(float delta);
   wxRibbonHSLColour Darker(float delta) const;
   wxRibbonHSLColour Lighter(float delta) const;
   wxRibbonHSLColour Saturated(float delta) const;
   wxRibbonHSLColour Desaturated(float delta) const;
   wxRibbonHSLColour ShiftHue(float delta) const;

   float       hue, saturation, luminance;
};

WXDLLIMPEXP_RIBBON wxRibbonHSLColour wxRibbonShiftLuminance(
                                wxRibbonHSLColour colour, float amount);

#endif // wxUSE_RIBBON

#endif // _WX_RIBBON_ART_INTERNAL_H_
