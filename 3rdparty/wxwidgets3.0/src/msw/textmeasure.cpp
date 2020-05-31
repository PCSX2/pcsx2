///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/textmeasure.cpp
// Purpose:     wxTextMeasure implementation for wxMSW
// Author:      Manuel Martin
// Created:     2012-10-05
// Copyright:   (c) 1997-2012 wxWidgets team
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

#include "wx/msw/private.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/font.h"
#endif //WX_PRECOMP

#include "wx/private/textmeasure.h"

#include "wx/msw/dc.h"

// ============================================================================
// wxTextMeasure implementation
// ============================================================================

void wxTextMeasure::Init()
{
    m_hdc = NULL;
    m_hfontOld = NULL;

    if ( m_dc )
    {
        wxClassInfo* const ci = m_dc->GetImpl()->GetClassInfo();

        if ( ci->IsKindOf(wxCLASSINFO(wxMSWDCImpl)))
        {
            m_useDCImpl = false;
        }
    }
}

void wxTextMeasure::BeginMeasuring()
{
    if ( m_dc )
    {
        m_hdc = m_dc->GetHDC();

        // Non-native wxDC subclasses should override their DoGetTextExtent()
        // and other methods.
        wxASSERT_MSG( m_hdc, wxS("Must not be used with non-native wxDCs") );
    }
    else if ( m_win )
    {
        m_hdc = ::GetDC(GetHwndOf(m_win));
    }

    // We need to set the font if it's explicitly specified, of course, but
    // also if we're associated with a window because the window HDC created
    // above has the default font selected into it and not the font of the
    // window.
    if ( m_font || m_win )
        m_hfontOld = (HFONT)::SelectObject(m_hdc, GetHfontOf(GetFont()));
}

void wxTextMeasure::EndMeasuring()
{
    if ( m_hfontOld )
    {
        ::SelectObject(m_hdc, m_hfontOld);
        m_hfontOld = NULL;
    }

    if ( m_win )
        ::ReleaseDC(GetHwndOf(m_win), m_hdc);
    //else: our HDC belongs to m_dc, don't touch it

    m_hdc = NULL;
}

// Notice we don't check here the font. It is supposed to be OK before the call.
void wxTextMeasure::DoGetTextExtent(const wxString& string,
                                       wxCoord *width,
                                       wxCoord *height,
                                       wxCoord *descent,
                                       wxCoord *externalLeading)
{
    SIZE sizeRect;
    const size_t len = string.length();
    if ( !::GetTextExtentPoint32(m_hdc, string.t_str(), len, &sizeRect) )
    {
        wxLogLastError(wxT("GetTextExtentPoint32()"));
    }

#if !defined(_WIN32_WCE) || (_WIN32_WCE >= 400)
    // the result computed by GetTextExtentPoint32() may be too small as it
    // accounts for under/overhang of the first/last character while we want
    // just the bounding rect for this string so adjust the width as needed
    // (using API not available in 2002 SDKs of WinCE)
    if ( len > 0 )
    {
        ABC widthABC;
        const wxChar chFirst = *string.begin();
        if ( ::GetCharABCWidths(m_hdc, chFirst, chFirst, &widthABC) )
        {
            if ( widthABC.abcA < 0 )
                sizeRect.cx -= widthABC.abcA;

            if ( len > 1 )
            {
                const wxChar chLast = *string.rbegin();
                ::GetCharABCWidths(m_hdc, chLast, chLast, &widthABC);
            }
            //else: we already have the width of the last character

            if ( widthABC.abcC < 0 )
                sizeRect.cx -= widthABC.abcC;
        }
        //else: GetCharABCWidths() failed, not a TrueType font?
    }
#endif // !defined(_WIN32_WCE) || (_WIN32_WCE >= 400)

    *width = sizeRect.cx;
    *height = sizeRect.cy;

    if ( descent || externalLeading )
    {
        TEXTMETRIC tm;
        ::GetTextMetrics(m_hdc, &tm);
        if ( descent )
            *descent = tm.tmDescent;
        if ( externalLeading )
            *externalLeading = tm.tmExternalLeading;
    }
}

bool wxTextMeasure::DoGetPartialTextExtents(const wxString& text,
                                            wxArrayInt& widths,
                                            double scaleX)
{
    if ( !m_hdc )
        return wxTextMeasureBase::DoGetPartialTextExtents(text, widths, scaleX);

    static int maxLenText = -1;
    static int maxWidth = -1;

    if (maxLenText == -1)
    {
        // Win9x and WinNT+ have different limits
        int version = wxGetOsVersion();
        maxLenText = version == wxOS_WINDOWS_NT ? 65535 : 8192;
        maxWidth =   version == wxOS_WINDOWS_NT ? INT_MAX : 32767;
    }

    int len = text.length();
    if ( len > maxLenText )
        len = maxLenText;

    int fit = 0;
    SIZE sz = {0,0};
    if ( !::GetTextExtentExPoint(m_hdc,
                                 text.t_str(), // string to check
                                 len,
                                 maxWidth,
                                 &fit,         // [out] count of chars
                                               // that will fit
                                 &widths[0],   // array to fill
                                 &sz) )
    {
        wxLogLastError(wxT("GetTextExtentExPoint"));

        return false;
    }

    return true;
}
