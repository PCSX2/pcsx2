///////////////////////////////////////////////////////////////////////////////
// Name:        src/ribbon/art_internal.cpp
// Purpose:     Helper functions & classes used by ribbon art providers
// Author:      Peter Cawley
// Modified by:
// Created:     2009-08-04
// Copyright:   (C) Peter Cawley
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_RIBBON

#include "wx/ribbon/art.h"
#include "wx/ribbon/art_internal.h"
#include "wx/ribbon/bar.h"
#include "wx/ribbon/buttonbar.h"
#include "wx/ribbon/gallery.h"

#ifndef WX_PRECOMP
#include "wx/dc.h"
#endif

#ifdef __WXMSW__
#include "wx/msw/private.h"
#endif

wxRibbonArtProvider::wxRibbonArtProvider() {}
wxRibbonArtProvider::~wxRibbonArtProvider() {}

wxColour wxRibbonInterpolateColour(const wxColour& start_colour,
                                const wxColour& end_colour,
                                int position,
                                int start_position,
                                int end_position)
{
    if(position <= start_position)
    {
        return start_colour;
    }
    if(position >= end_position)
    {
        return end_colour;
    }
    position -= start_position;
    end_position -= start_position;
    int r = end_colour.Red() - start_colour.Red();
    int g = end_colour.Green() - start_colour.Green();
    int b = end_colour.Blue() - start_colour.Blue();
    r = start_colour.Red()   + (((r * position * 100) / end_position) / 100);
    g = start_colour.Green() + (((g * position * 100) / end_position) / 100);
    b = start_colour.Blue()  + (((b * position * 100) / end_position) / 100);
    return wxColour(r, g, b);
}

bool wxRibbonCanLabelBreakAtPosition(const wxString& label, size_t pos)
{
    return label[pos] == ' ';
}

void wxRibbonDrawParallelGradientLines(wxDC& dc,
                                    int nlines,
                                    const wxPoint* line_origins,
                                    int stepx,
                                    int stepy,
                                    int numsteps,
                                    int offset_x,
                                    int offset_y,
                                    const wxColour& start_colour,
                                    const wxColour& end_colour)
{
    int rd, gd, bd;
    rd = end_colour.Red() - start_colour.Red();
    gd = end_colour.Green() - start_colour.Green();
    bd = end_colour.Blue() - start_colour.Blue();

    for (int step = 0; step < numsteps; ++step)
    {
        int r,g,b;

        r = start_colour.Red() + (((step*rd*100)/numsteps)/100);
        g = start_colour.Green() + (((step*gd*100)/numsteps)/100);
        b = start_colour.Blue() + (((step*bd*100)/numsteps)/100);

        wxPen p(wxColour((unsigned char)r,
                        (unsigned char)g,
                        (unsigned char)b));
        dc.SetPen(p);

        for(int n = 0; n < nlines; ++n)
        {
            dc.DrawLine(offset_x + line_origins[n].x, offset_y + line_origins[n].y,
                        offset_x + line_origins[n].x + stepx, offset_y + line_origins[n].y + stepy);
        }

        offset_x += stepx;
        offset_y += stepy;
    }
}

wxRibbonHSLColour wxRibbonShiftLuminance(wxRibbonHSLColour colour,
                                                float amount)
{
    if(amount <= 1.0f)
        return colour.Darker(colour.luminance * (1.0f - amount));
    else
        return colour.Lighter((1.0f - colour.luminance) * (amount - 1.0f));
}

wxBitmap wxRibbonLoadPixmap(const char* const* bits, wxColour fore)
{
    wxImage xpm = wxBitmap(bits).ConvertToImage();
    xpm.Replace(255, 0, 255, fore.Red(), fore.Green(), fore.Blue());
    return wxBitmap(xpm);
}

wxRibbonHSLColour::wxRibbonHSLColour(const wxColour& col)
{
    float red = float(col.Red()) / 255.0;
    float green = float(col.Green()) / 255.0;
    float blue = float(col.Blue()) / 255.0;
    float Min = wxMin(red, wxMin(green, blue));
    float Max = wxMax(red, wxMax(green, blue));
    luminance = 0.5 * (Max + Min);
    if (Min == Max)
    {
        // colour is a shade of grey
        hue = 0.0;
        saturation = 0.0;
    }
    else
    {
        if(luminance <= 0.5)
            saturation = (Max - Min) / (Max + Min);
        else
            saturation = (Max - Min) / (2.0 - (Max + Min));

        if(Max == red)
        {
            hue = 60.0 * (green - blue) / (Max - Min);
            if(hue < 0.0)
                hue += 360.0;
        }
        else if(Max == green)
        {
            hue = 60.0 * (blue - red) / (Max - Min);
            hue += 120.0;
        }
        else // Max == blue
        {
            hue = 60.0 * (red - green) / (Max - Min);
            hue += 240.0;
        }
    }
}

wxColour wxRibbonHSLColour::ToRGB() const
{
    float _hue = (hue - floor(hue / 360.0f) * 360.0f);
    float _saturation = saturation;
    float _luminance = luminance;
    if(_saturation > 1.0) _saturation = 1.0;
    if(_saturation < 0.0) _saturation = 0.0;
    if(_luminance > 1.0) _luminance = 1.0;
    if(_luminance < 0.0) _luminance = 0.0;

    float red, blue, green;
    if(_saturation == 0.0)
    {
        // colour is a shade of grey
        red = blue = green = _luminance;
    }
    else
    {
        double tmp2 = (_luminance < 0.5)
           ? _luminance * (1.0 + _saturation)
           : (_luminance + _saturation) - (_luminance * _saturation);
        double tmp1 = 2.0 * _luminance - tmp2;

        double tmp3R = _hue + 120.0;
        if(tmp3R > 360.0)
            tmp3R -= 360.0;
        if(tmp3R < 60.0)
            red = tmp1 + (tmp2 - tmp1) * tmp3R / 60.0;
        else if(tmp3R < 180.0)
            red = tmp2;
        else if(tmp3R < 240.0)
            red = tmp1 + (tmp2 - tmp1) * (240.0 - tmp3R) / 60.0;
        else
            red = tmp1;

        double tmp3G = _hue;
        if(tmp3G > 360.0)
            tmp3G -= 360.0;
        if(tmp3G < 60.0)
            green = tmp1 + (tmp2 - tmp1) * tmp3G / 60.0;
        else if(tmp3G < 180.0)
            green = tmp2;
        else if(tmp3G < 240.0)
            green = tmp1 + (tmp2 - tmp1) * (240.0 - tmp3G) / 60.0;
        else
            green = tmp1;

        double tmp3B = _hue + 240.0;
        if(tmp3B > 360.0)
            tmp3B -= 360.0;
        if(tmp3B < 60.0)
            blue = tmp1 + (tmp2 - tmp1) * tmp3B / 60.0;
        else if(tmp3B < 180.0)
            blue = tmp2;
        else if(tmp3B < 240.0)
            blue = tmp1 + (tmp2 - tmp1) * (240.0 - tmp3B) / 60.0;
        else
            blue = tmp1;
    }
    return wxColour(
        (unsigned char)(red * 255.0),
        (unsigned char)(green * 255.0),
        (unsigned char)(blue * 255.0));
}

wxRibbonHSLColour wxRibbonHSLColour::Darker(float delta) const
{
    return Lighter(-delta);
}

wxRibbonHSLColour& wxRibbonHSLColour::MakeDarker(float delta)
{
    luminance -= delta;
    return *this;
}

wxRibbonHSLColour wxRibbonHSLColour::Lighter(float delta) const
{
    return wxRibbonHSLColour(hue, saturation, luminance + delta);
}

wxRibbonHSLColour wxRibbonHSLColour::Saturated(float delta) const
{
    return wxRibbonHSLColour(hue, saturation + delta, luminance);
}

wxRibbonHSLColour wxRibbonHSLColour::Desaturated(float delta) const
{
    return Saturated(-delta);
}

wxRibbonHSLColour wxRibbonHSLColour::ShiftHue(float delta) const
{
    return wxRibbonHSLColour(hue + delta, saturation, luminance);
}

#endif // wxUSE_RIBBON
